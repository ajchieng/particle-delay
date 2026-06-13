#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <array>
#include <cmath>
#include "Particle.h"
#include "EchoEvent.h"

//==============================================================================
// Owns the live set of particles and advances the physics one control tick at a
// time. A transient queues a burst of particles that are released gradually from
// the centre; gravity drops them onto the floor, and every floor bounce appends
// an EchoEvent that the processor renders into audio.
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
    void triggerBurst (int count, float scatter);

    // Advance every particle one control tick: release any due particles, then
    // integrate motion. Each floor bounce appends an EchoEvent.
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

    // Height particles are dropped from (y = 0 top .. 1 floor). The drop distance
    // is (1 - spawnHeight), which the tempo-sync math below uses.
    static constexpr float spawnHeight = 0.5f;

    // Per-tick gravity that makes a rest-drop from spawnHeight reach the floor in
    // exactly 'fallSeconds'. Used by the processor to lock the first bounce to a
    // tempo division. Discrete Euler: displacement after n ticks = g*n(n+1)/2.
    static float gravityForFallTime (double fallSeconds, double controlRateHz, float dropHeight)
    {
        const double n        = juce::jmax (1.0, std::round (fallSeconds * controlRateHz));
        const double distance = juce::jlimit (0.0, 1.0, 1.0 - (double) dropHeight);
        return (float) (2.0 * distance / (n * (n + 1.0)));
    }

private:
    // Build one centre-dropped particle (allocation-free: reuses a recycled slot
    // when the store is full).
    void releaseOne (float scatter);

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
    double controlRateHz = 250.0;   // ticks/sec; delay clock (ticks) -> ms uses this
    float  maxAgeTicks   = 2000.0f; // hard lifetime cap (~8 s at the 250 Hz control rate)

    // Staggered release of a triggered burst.
    int    pendingReleases = 0;     // particles still waiting to be dropped
    float  releaseScatter  = 0.5f;  // scatter captured when the burst was triggered
    double releaseTimer    = 0.0;   // ticks since the last release
    double ticksSinceBurst = 0.0;   // anchors staggered particles to the source hit

    // Tuning. Spread = horizontal launch velocity; releaseInterval = ticks
    // between drops; minBounceVel = below this a settled particle is killed so it
    // can't buzz audio-rate echoes as the bounces shrink to nothing.
    static constexpr float  maxSpread         = 0.010f;  // horizontal kick at scatter = 1
    static constexpr double minReleaseTicks   = 1.0;     // near-instant burst at scatter = 0
    static constexpr double maxReleaseTicks   = 80.0;    // ~0.32 s window at scatter = 1
    static constexpr float  minBounceVel      = 0.004f;  // settle threshold

    std::vector<Particle> particles;
    juce::Random random;

    // Lock-free-ish snapshot for the visualiser. The audio thread writes plain
    // floats here after each update; the GUI reads them. Individual values may
    // tear, which is harmless for drawing dots, and the count is atomic so the
    // GUI never reads past valid data.
    static constexpr int snapshotCapacity = maxParticles;
    std::array<ParticleSnapshot, snapshotCapacity> snapshot;
    std::atomic<int> snapshotCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleSystem)
};
