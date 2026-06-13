#include "ParticleSystem.h"
#include <algorithm>
#include <cmath>

ParticleSystem::ParticleSystem()
{
    particles.reserve (maxParticles);
}

void ParticleSystem::prepare (double newSampleRate, double newControlRateHz)
{
    sampleRate    = newSampleRate;
    controlRateHz = newControlRateHz > 0.0 ? newControlRateHz : 250.0;
    particles.reserve (maxParticles);
    reset();
}

void ParticleSystem::reset()
{
    particles.clear();
    pendingReleases = 0;
    releaseTimer    = 0.0;
    ticksSinceBurst = 0.0;
    snapshotCount.store (0, std::memory_order_relaxed);
}

void ParticleSystem::triggerBurst (int count, float scatter)
{
    pendingReleases = juce::jlimit (1, maxParticles, count);
    releaseScatter  = juce::jlimit (0.0f, 1.0f, scatter);
    // Seed the timer at one interval so the first particle drops on the next tick.
    releaseTimer    = releaseIntervalTicks (releaseScatter);
    ticksSinceBurst = 0.0;
}

void ParticleSystem::releaseOne (float scatter)
{
    Particle p;

    // Dropped from the centre with a horizontal kick that fans the burst out;
    // wider kick as scatter rises. Drop height is fixed (no vertical jitter) so
    // the fall time stays locked to the tempo-synced gravity.
    p.x = juce::jlimit (0.0f, 1.0f, 0.5f + randomSigned() * 0.02f * scatter);
    p.y = spawnHeight;

    p.vx = randomSigned() * maxSpread * scatter;
    p.vy = 0.0f;   // released from rest; gravity does the rest

    p.energy        = 1.0f;
    p.age           = 0.0f;
    // update() increments both clocks during this tick, so start one tick behind
    // the burst clock to keep the particle aligned after integration.
    p.delayClockTicks = (float) juce::jmax (0.0, ticksSinceBurst - 1.0);
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
    // Gravity is computed from host tempo (see PluginProcessor), so allow a wide
    // range - short divisions at fast tempos need a much stronger pull.
    gravity = juce::jlimit (0.0f, 0.05f, gravity);
    bounce  = juce::jlimit (0.1f,  0.99f, bounce);
    decay   = juce::jlimit (0.90f, 0.9999f, decay);

    ticksSinceBurst += 1.0;

    // ---- Staggered release: trickle out the queued burst -----------------------
    if (pendingReleases > 0)
    {
        const double interval = releaseIntervalTicks (releaseScatter);
        releaseTimer += 1.0;
        while (pendingReleases > 0 && releaseTimer >= interval)
        {
            releaseTimer -= interval;
            releaseOne (releaseScatter);
            --pendingReleases;
        }
    }

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
        p.delayClockTicks += 1.0f;

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
        snapshot[(size_t) i].x      = particles[(size_t) i].x;
        snapshot[(size_t) i].y      = particles[(size_t) i].y;
        snapshot[(size_t) i].energy = particles[(size_t) i].energy;
    }
    snapshotCount.store (n, std::memory_order_relaxed);
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

    const float dMin = juce::jmin (delayMinMs, delayMaxMs);
    const float dMax = juce::jmax (delayMinMs, delayMaxMs);

    EchoEvent e;
    e.pan = juce::jlimit (0.0f, 1.0f, p.x);

    // Delay time = elapsed time since the source hit. At every bounce, reading
    // this far back lands on that hit instead of on the previous bounce.
    const float elapsedMs = p.delayClockTicks * 1000.0f / (float) controlRateHz;
    e.delayMs = juce::jlimit (dMin, dMax, elapsedMs);

    e.gain = juce::jlimit (0.0f, 1.0f, p.energy);

    // Harder slam -> brighter echo.
    e.brightness = juce::jlimit (0.0f, 1.0f, impactSpeed * 120.0f);

    out.push_back (e);
}

int ParticleSystem::getSnapshot (ParticleSnapshot* dest, int maxOut) const
{
    const int n = juce::jlimit (0, maxOut,
                                juce::jmin (snapshotCount.load (std::memory_order_relaxed),
                                            snapshotCapacity));
    for (int i = 0; i < n; ++i)
        dest[i] = snapshot[(size_t) i];

    return n;
}
