// Offline functional checks for the Particle Delay DSP core. Not part of the
// plugin: build with -DPARTICLEDELAY_BUILD_TESTS=ON and run ParticleDelayTests.
//
// These prove the end-to-end claim without a DAW: a transient is detected, it
// releases particles that bounce on the floor, and each bounce yields an echo
// with sane pan / delay / gain, which the delay line can read back.

#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <vector>

#include "../Source/DSP/DelayBuffer.h"
#include "../Source/DSP/TransientDetector.h"
#include "../Source/DSP/ParticleSystem.h"
#include "../Source/DSP/EchoEvent.h"
#include "../Source/DSP/DelaySync.h"

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", condition ? "PASS" : "FAIL", what.toRawUTF8());
        if (! condition)
            ++failures;
    }
}

int main()
{
    constexpr double sr = 48000.0;

    std::printf ("DelayBuffer:\n");
    {
        DelayBuffer db;
        db.prepare (sr, 2, 2200.0f);

        // Write a known ramp, then read back a whole-sample delay.
        const int n = 1000;
        for (int i = 0; i < n; ++i)
        {
            const float v = (float) i;
            db.writeFrame (v, -v);
        }
        // 10 samples back from the last write (i==999) is sample 989.
        const float delayMs = 10.0f / (float) sr * 1000.0f;
        const float l = db.readSample (0, delayMs);
        const float r = db.readSample (1, delayMs);
        check (std::abs (l - 989.0f) < 0.01f, "reads back the expected past sample (L)");
        check (std::abs (r + 989.0f) < 0.01f, "reads back the expected past sample (R)");

        // Out-of-range delay is clamped, never crashes.
        const float clamped = db.readSample (0, 999999.0f);
        check (std::isfinite (clamped), "over-range delay is clamped and finite");
    }

    std::printf ("DelaySync:\n");
    {
        check (std::abs (DelaySync::milliseconds (7, 120.0) - 250.0f) < 0.01f,
               "1/8 note converts to 250 ms at 120 BPM");
        check (std::abs (DelaySync::milliseconds (9, 120.0) - 500.0f) < 0.01f,
               "1/4 note converts to 500 ms at 120 BPM");
        check (std::abs (DelaySync::milliseconds (12, 20.0) - 12000.0f) < 0.01f,
               "1/1 note fits the 12-second range at the minimum BPM");
    }

    std::printf ("TransientDetector:\n");
    {
        TransientDetector det;
        det.prepare (sr);

        int hits = 0;
        for (int i = 0; i < (int) sr; ++i) // 1 second
        {
            // One loud click at the very start, silence afterwards.
            const float s = (i == 5) ? 0.9f : 0.0f;
            if (det.processSample (s, s, 0.15f))
                ++hits;
        }
        check (hits == 1, "a single click fires exactly one transient");

        // Sustained tone below threshold should not fire.
        det.reset();
        int quietHits = 0;
        for (int i = 0; i < (int) sr; ++i)
        {
            const float s = 0.05f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / sr);
            if (det.processSample (s, s, 0.15f))
                ++quietHits;
        }
        check (quietHits == 0, "quiet signal below threshold never fires");
    }

    std::printf ("ParticleSystem:\n");
    {
        constexpr double controlRate = 250.0;

        ParticleSystem ps;
        ps.prepare (sr, controlRate);

        std::vector<EchoEvent> events;
        events.reserve (ParticleSystem::maxParticles * 2 + 16);

        // Queue a burst from the centre; update() releases it gradually.
        ps.triggerBurst (8, 0.5f);

        int totalEchoes = 0;
        float minPan = 1.0f, maxPan = 0.0f;
        float minDelay = 1.0e9f, maxDelay = -1.0e9f;
        bool gainInRange = true;

        // Run plenty of control ticks for the staggered burst to drain and the
        // particles to bounce, settle and die.
        for (int tick = 0; tick < 4000; ++tick)
        {
            events.clear();
            ps.update (0.00035f, 0.72f, 0.995f, 60.0f, 650.0f, events);

            for (const auto& e : events)
            {
                ++totalEchoes;
                minPan = juce::jmin (minPan, e.pan);
                maxPan = juce::jmax (maxPan, e.pan);
                minDelay = juce::jmin (minDelay, e.delayMs);
                maxDelay = juce::jmax (maxDelay, e.delayMs);
                if (e.gain < 0.0f || e.gain > 1.0f) gainInRange = false;
            }
        }

        check (totalEchoes > 0, "bouncing particles generate echoes");
        check (gainInRange, "all echo gains stay within [0, 1]");
        check (minPan >= 0.0f && maxPan <= 1.0f, "pan stays within [0, 1]");
        check (minDelay >= 60.0f - 0.01f && maxDelay <= 650.0f + 0.01f,
               "delay times stay within [Delay Min, Delay Max]");
        check (maxDelay - minDelay > 1.0f,
               "time-since-hit mapping yields a range of delay times (not all pinned)");

        // Particles must eventually all die (energy decay / age cap).
        ParticleSystem::ParticleSnapshot snap[ParticleSystem::maxParticles];
        const int aliveAfter = ps.getSnapshot (snap, ParticleSystem::maxParticles);
        check (aliveAfter == 0, "all particles eventually die");

        std::printf ("    (generated %d echoes; delay range %.0f..%.0f ms)\n",
                     totalEchoes, minDelay, maxDelay);
    }

    std::printf ("Tempo sync:\n");
    {
        constexpr double controlRate = 250.0;

        ParticleSystem ps;
        ps.prepare (sr, controlRate);

        std::vector<EchoEvent> events;
        events.reserve (ParticleSystem::maxParticles * 2 + 16);

        // 1/8 note at 120 BPM = 0.25 s. Gravity is derived so a centre-drop
        // reaches the floor in exactly that time.
        const double fallSeconds = 0.25;
        const float  g = ParticleSystem::gravityForFallTime (fallSeconds, controlRate,
                                                             ParticleSystem::spawnHeight);

        ps.triggerBurst (1, 0.0f);   // one particle, no scatter -> dropped straight down

        int   firstEchoTick   = -1;
        float firstEchoDelay  = -1.0f;
        float secondEchoDelay = -1.0f;
        for (int tick = 0; tick < 2000; ++tick)
        {
            events.clear();
            ps.update (g, 0.72f, 0.9999f, 10.0f, 1000.0f, events);
            for (const auto& e : events)
            {
                if (firstEchoDelay < 0.0f)
                {
                    firstEchoTick  = tick;
                    firstEchoDelay = e.delayMs;
                }
                else
                {
                    secondEchoDelay = e.delayMs;
                    break;
                }
            }

            if (secondEchoDelay >= 0.0f)
                break;
        }

        const double expectedTicks = fallSeconds * controlRate;   // ~62.5
        const double expectedMs    = fallSeconds * 1000.0;        // 250 ms
        check (firstEchoTick >= 0 && std::abs ((double) firstEchoTick - expectedTicks) <= 2.0,
               "first bounce fires on the tempo-synced fall time");
        check (firstEchoDelay > 0.0f && std::abs ((double) firstEchoDelay - expectedMs) <= 8.0,
               "first echo's delay equals time since the hit (the synced division)");
        check (secondEchoDelay > firstEchoDelay,
               "later bounces use cumulative time since the hit");

        std::printf ("    (first bounce at tick %d, expected ~%.0f; delays %.0f then %.0f ms)\n",
                     firstEchoTick, expectedTicks, firstEchoDelay, secondEchoDelay);
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return failures == 0 ? 0 : 1;
}
