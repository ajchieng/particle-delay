#include "ParticleSystem.h"
#include <algorithm>
#include <cmath>

ParticleSystem::ParticleSystem()
{
    particles.reserve (maxParticles);
    pendingBursts.reserve (maxPendingBursts);
}

void ParticleSystem::prepare (double newSampleRate, double newControlRateHz)
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 0.0
        ? newSampleRate
        : 44100.0;
    controlRateHz = std::isfinite (newControlRateHz) && newControlRateHz > 0.0
        ? newControlRateHz
        : 250.0;
    particles.reserve (maxParticles);
    pendingBursts.reserve (maxPendingBursts);
    reset();
}

void ParticleSystem::reset()
{
    particles.clear();
    pendingBursts.clear();
    snapshotCount.store (0, std::memory_order_release);
}

void ParticleSystem::triggerBurst (int count, float scatter, uint64_t sourceId)
{
    if (sourceId == 0)
        return;

    if ((int) pendingBursts.size() >= maxPendingBursts)
        pendingBursts.erase (pendingBursts.begin());

    PendingBurst burst;
    burst.sourceId = sourceId;
    burst.remaining = juce::jlimit (1, maxParticles, count);
    burst.scatter = juce::jlimit (0.0f, 1.0f, scatter);
    // Seed the timer at one interval so the first particle drops on the next tick.
    burst.releaseTimer = releaseIntervalTicks (burst.scatter);
    pendingBursts.push_back (burst);
}

void ParticleSystem::retireSource (uint64_t sourceId)
{
    if (sourceId == 0)
        return;

    pendingBursts.erase (
        std::remove_if (pendingBursts.begin(), pendingBursts.end(),
                        [sourceId] (const PendingBurst& burst)
                        {
                            return burst.sourceId == sourceId;
                        }),
        pendingBursts.end());

    particles.erase (
        std::remove_if (particles.begin(), particles.end(),
                        [sourceId] (const Particle& particle)
                        {
                            return particle.sourceId == sourceId;
                        }),
        particles.end());
}

void ParticleSystem::releaseOne (const PendingBurst& burst)
{
    Particle p;

    // Dropped from the centre with a horizontal kick that fans the burst out;
    // wider kick as scatter rises. Drop height is fixed (no vertical jitter) so
    // a given Gravity value produces consistent first-impact timing.
    p.x = juce::jlimit (0.0f, 1.0f, 0.5f + randomSigned() * 0.02f * burst.scatter);
    p.y = spawnHeight;

    p.vx = randomSigned() * maxSpread * burst.scatter;
    p.vy = 0.0f;   // released from rest; gravity does the rest

    p.energy        = 1.0f;
    p.age           = 0.0f;
    p.sourceId      = burst.sourceId;
    // update() increments the particle during this tick, so start one tick
    // behind the burst clock to keep both elapsed times aligned.
    p.elapsedTicks  = (float) juce::jmax (0.0, burst.elapsedTicks - 1.0);
    p.alive         = true;

    if ((int) particles.size() < maxParticles)
    {
        particles.push_back (p);
    }
    else
    {
        // At capacity: recycle the weakest particle's slot in place so we never
        // allocate on the audio thread.
        int weakest = 0;
        for (int j = 1; j < (int) particles.size(); ++j)
            if (particles[(size_t) j].energy < particles[(size_t) weakest].energy)
                weakest = j;

        particles[(size_t) weakest] = p;
    }
}

void ParticleSystem::update (float gravity,
                             float bounce,
                             float decay,
                             float delayMinMs,
                             float delayMaxMs,
                             std::vector<EchoEvent>& echoEvents)
{
    // The 0.0625x-16x user range stays comfortably below this defensive cap.
    gravity = juce::jlimit (0.0f, 0.05f, gravity);
    bounce  = juce::jlimit (0.1f,  0.99f, bounce);
    decay   = juce::jlimit (0.90f, 0.9999f, decay);

    // ---- Staggered releases: advance every active burst independently ----------
    for (auto& burst : pendingBursts)
    {
        burst.elapsedTicks += 1.0;
        const double interval = releaseIntervalTicks (burst.scatter);
        burst.releaseTimer += 1.0;
        while (burst.remaining > 0 && burst.releaseTimer >= interval)
        {
            burst.releaseTimer -= interval;
            releaseOne (burst);
            --burst.remaining;
        }
    }
    pendingBursts.erase (
        std::remove_if (pendingBursts.begin(), pendingBursts.end(),
                        [] (const PendingBurst& burst) { return burst.remaining <= 0; }),
        pendingBursts.end());

    // ---- Integrate motion ------------------------------------------------------
    for (auto& p : particles)
    {
        if (! p.alive)
            continue;

        p.vy += gravity;
        p.vx = juce::jlimit (-0.5f, 0.5f, p.vx);
        p.vy = juce::jlimit (-0.5f, 0.5f, p.vy);

        p.x += p.vx;
        p.y += p.vy;

        p.energy        *= decay;
        p.age           += 1.0f;
        p.elapsedTicks += 1.0f;

        // Side edges: no walls. Clamp position so particles don't fly off; pan
        // just saturates hard left/right. No reflection, no echo.
        if (p.x < 0.0f) p.x = 0.0f;
        if (p.x > 1.0f) p.x = 1.0f;

        // Defensive ceiling clamp (a rest-dropped particle never reaches it).
        if (p.y < 0.0f) { p.y = 0.0f; if (p.vy < 0.0f) p.vy = 0.0f; }

        // Floor (y = 1): the only collision that bounces and fires an echo.
        if (p.y >= 1.0f)
        {
            p.y = 1.0f;

            const float impactSpeed = std::abs (p.vy);
            createEchoEvent (p, impactSpeed, delayMinMs, delayMaxMs, echoEvents);

            p.vy     = -p.vy * bounce;   // reflect upward, losing energy
            p.energy *= bounce;

            // Settled: the bounce is too small to matter - let it rest (and stop
            // it machine-gunning audio-rate echoes as the height shrinks to zero).
            if (std::abs (p.vy) < minBounceVel)
                p.alive = false;
        }

        if (p.energy < 0.005f || p.age > maxAgeTicks)
            p.alive = false;
    }

    // Drop dead particles (in place, no allocation).
    particles.erase (std::remove_if (particles.begin(), particles.end(),
                                     [] (const Particle& p) { return ! p.alive; }),
                     particles.end());

    // Publish a snapshot for the visualiser.
    const int n = juce::jmin ((int) particles.size(), snapshotCapacity);
    for (int i = 0; i < n; ++i)
    {
        snapshot[(size_t) i].x.store (particles[(size_t) i].x,
                                      std::memory_order_relaxed);
        snapshot[(size_t) i].y.store (particles[(size_t) i].y,
                                      std::memory_order_relaxed);
        snapshot[(size_t) i].energy.store (particles[(size_t) i].energy,
                                           std::memory_order_relaxed);
    }
    snapshotCount.store (n, std::memory_order_release);
}

void ParticleSystem::createEchoEvent (const Particle& p,
                                      float impactSpeed,
                                      float delayMinMs,
                                      float delayMaxMs,
                                      std::vector<EchoEvent>& out) const
{
    // Don't grow past the reserved capacity on the audio thread.
    if ((int) out.size() >= (int) out.capacity())
        return;

    const float windowStart = juce::jmin (delayMinMs, delayMaxMs);
    const float windowEnd = juce::jmax (delayMinMs, delayMaxMs);
    const float elapsedMs = p.elapsedTicks * 1000.0f / (float) controlRateHz;
    if (elapsedMs < windowStart || elapsedMs > windowEnd)
        return;

    EchoEvent e;
    e.sourceId = p.sourceId;
    e.pan = juce::jlimit (0.0f, 1.0f, p.x);
    e.elapsedMs = elapsedMs;

    e.gain = juce::jlimit (0.0f, 1.0f, p.energy);

    // Harder slam -> brighter echo.
    e.brightness = juce::jlimit (0.0f, 1.0f, impactSpeed * 120.0f);

    out.push_back (e);
}

int ParticleSystem::getSnapshot (ParticleSnapshot* dest, int maxOut) const
{
    const int n = juce::jlimit (0, maxOut,
                                juce::jmin (snapshotCount.load (std::memory_order_acquire),
                                            snapshotCapacity));
    for (int i = 0; i < n; ++i)
    {
        dest[i].x = snapshot[(size_t) i].x.load (std::memory_order_relaxed);
        dest[i].y = snapshot[(size_t) i].y.load (std::memory_order_relaxed);
        dest[i].energy = snapshot[(size_t) i].energy.load (std::memory_order_relaxed);
    }

    return n;
}
