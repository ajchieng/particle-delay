#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include "Particle.h"
#include "EchoEvent.h"

//==============================================================================
// Owns the live set of particles and advances the physics one control tick at a
// time. A transient queues a burst of particles that are released gradually from
// the centre; gravity drops them onto the floor, and every floor bounce inside
// the audible time window appends an EchoEvent for captured-hit playback.
//
// All real-time methods (triggerBurst/update) are allocation-free once prepared:
// the particle store is pre-reserved and capped, and full slots are recycled in
// place rather than grown.
class ParticleSystem
{
public:
    // A read-only copy of a particle for the GUI visualiser.
    struct ParticleSnapshot
    {
        float x = 0.5f;
        float y = 0.5f;
        float energy = 0.0f;
    };

    ParticleSystem();

    void prepare (double sampleRate, double controlRateHz);
    void reset();

    // Queue 'count' particles to be released from the centre over a short window.
    // Higher scatter widens both the spatial fan-out and the release window.
    // Does not spawn immediately - update() trickles them out tick by tick.
    void triggerBurst (int count, float scatter, uint64_t sourceId = 1);

    // Retire all queued and live particles belonging to a recycled capture.
    void retireSource (uint64_t sourceId);

    // Advance every particle one control tick: release any due particles, then
    // integrate motion. Floor bounces inside the audible window append events.
    void update (float gravity,
                 float bounce,
                 float decay,
                 float delayMinMs,
                 float delayMaxMs,
                 std::vector<EchoEvent>& echoEvents);

    // ---- GUI side (call from the message thread) -----------------------------
    // Copies up to maxOut particles into dest; returns how many were written.
    int getSnapshot (ParticleSnapshot* dest, int maxOut) const;

    static constexpr int maxParticles = 200;

    // Height particles are dropped from (y = 0 top .. 1 floor).
    static constexpr float spawnHeight = 0.5f;

    static constexpr float minimumGravityMultiplier = 0.0625f;
    static constexpr float maximumGravityMultiplier = 16.0f;
    static constexpr float defaultGravityMultiplier = 1.0f;
    static constexpr double neutralFallSeconds = 0.25;

    // Per-tick gravity that makes a rest-drop from spawnHeight reach the floor in
    // exactly 'fallSeconds'. Discrete Euler: displacement after n ticks =
    // g*n(n+1)/2.
    static float gravityForFallTime (double fallSeconds, double controlRateHz, float dropHeight)
    {
        const double n        = juce::jmax (1.0, std::round (fallSeconds * controlRateHz));
        const double distance = juce::jlimit (0.0, 1.0, 1.0 - (double) dropHeight);
        return (float) (2.0 * distance / (n * (n + 1.0)));
    }

    // Free-running gravity with 1.0x calibrated to a 250 ms first impact.
    // This deliberately has no host-tempo input.
    static float gravityForMultiplier (float multiplier,
                                       double controlRateHz,
                                       float dropHeight)
    {
        return gravityForFallTime (neutralFallSeconds, controlRateHz, dropHeight)
             * juce::jlimit (minimumGravityMultiplier,
                             maximumGravityMultiplier,
                             multiplier);
    }

private:
    struct AtomicParticleSnapshot
    {
        std::atomic<float> x { 0.5f };
        std::atomic<float> y { 0.5f };
        std::atomic<float> energy { 0.0f };
    };

    // Build one centre-dropped particle (allocation-free: reuses a recycled slot
    // when the store is full).
    struct PendingBurst
    {
        uint64_t sourceId = 0;
        int remaining = 0;
        float scatter = 0.5f;
        double releaseTimer = 0.0;
        double elapsedTicks = 0.0;
    };

    void releaseOne (const PendingBurst& burst);

    // Control ticks between staggered releases (wider window as scatter rises).
    double releaseIntervalTicks (float scatter) const
    {
        return juce::jmap ((double) juce::jlimit (0.0f, 1.0f, scatter),
                           0.0, 1.0, minReleaseTicks, maxReleaseTicks);
    }

    void createEchoEvent (const Particle& p,
                          float impactSpeed,
                          float delayMinMs,
                          float delayMaxMs,
                          std::vector<EchoEvent>& out) const;

    float randomSigned() { return random.nextFloat() * 2.0f - 1.0f; }

    double sampleRate    = 44100.0;
    double controlRateHz = 250.0;   // ticks/sec; elapsed bounce time uses this
    float  maxAgeTicks   = 2000.0f; // hard lifetime cap (~8 s at the 250 Hz control rate)

    // Independent staggered release state for overlapping detected hits.
    static constexpr int maxPendingBursts = 16;
    std::vector<PendingBurst> pendingBursts;

    // Tuning. Spread = horizontal launch velocity; releaseInterval = ticks
    // between drops; minBounceVel = below this a settled particle is killed so it
    // can't buzz audio-rate echoes as the bounces shrink to nothing.
    static constexpr float  maxSpread         = 0.010f;  // horizontal kick at scatter = 1
    static constexpr double minReleaseTicks   = 1.0;     // near-instant burst at scatter = 0
    static constexpr double maxReleaseTicks   = 80.0;    // ~0.32 s window at scatter = 1
    static constexpr float  minBounceVel      = 0.001f;  // supports rebounds at 0.0625x gravity

    std::vector<Particle> particles;
    juce::Random random;

    // Lock-free snapshot for the visualiser. Atomic fields avoid a data race
    // between the audio and message threads without ever blocking audio.
    static constexpr int snapshotCapacity = maxParticles;
    std::array<AtomicParticleSnapshot, snapshotCapacity> snapshot;
    std::atomic<int> snapshotCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleSystem)
};
