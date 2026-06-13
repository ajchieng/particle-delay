// Offline functional checks for the Particle Delay DSP core. Not part of the
// plugin: build with -DPARTICLEDELAY_BUILD_TESTS=ON and run ParticleDelayTests.
//
// These prove the end-to-end claim without a DAW: a transient is detected, it
// captures its stereo source, releases particles that bounce on the floor, and
// each audible-window bounce replays the correct captured hit.

#include <JuceHeader.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

#include "../Source/PluginProcessor.h"
#include "../Source/DSP/DelayBuffer.h"
#include "../Source/DSP/TransientDetector.h"
#include "../Source/DSP/ParticleSystem.h"
#include "../Source/DSP/EchoEvent.h"
#include "../Source/DSP/DelaySync.h"
#include "../Source/DSP/CapturedHit.h"

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
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
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

    std::printf ("CapturedHitBank:\n");
    {
        CapturedHitBank bank;
        bank.prepare (1000.0);

        const auto decaying = bank.startCapture (250.0f);
        bank.processSample (0.8f, -0.4f);
        for (int i = 0; i < 120; ++i)
            bank.processSample (0.0f, 0.0f);

        check (! bank.isRecording (decaying.sourceId),
               "decaying hit finishes after its 80 ms minimum and 20 ms silence");
        check (bank.getAvailableSamples (decaying.sourceId) == 80,
               "adaptive capture trims the trailing silence");
        check (std::abs (bank.getSample (decaying.sourceId, 0, 0) - 0.8f) < 0.001f
                   && std::abs (bank.getSample (decaying.sourceId, 1, 0) + 0.4f) < 0.001f,
               "captured hits preserve independent stereo samples");

        const auto sustained = bank.startCapture (100.0f);
        for (int i = 0; i < 150; ++i)
            bank.processSample (0.25f, 0.5f);
        check (! bank.isRecording (sustained.sourceId)
                   && bank.getAvailableSamples (sustained.sourceId) == 100,
               "sustained input reaches the selected capture maximum");

        bank.reset();
        const auto firstRhythmicHit = bank.startCapture (250.0f);
        for (int i = 0; i < 90; ++i)
            bank.processSample (0.2f, 0.1f);
        bank.finishMatureCaptures();
        const auto secondRhythmicHit = bank.startCapture (250.0f);
        bank.processSample (0.9f, 0.7f);
        check (! bank.isRecording (firstRhythmicHit.sourceId)
                   && bank.getAvailableSamples (firstRhythmicHit.sourceId) == 90
                   && std::abs (bank.getSample (firstRhythmicHit.sourceId, 0, 89) - 0.2f) < 0.001f,
               "a new onset closes a mature previous capture at the hit boundary");
        check (std::abs (bank.getSample (secondRhythmicHit.sourceId, 0, 0) - 0.9f) < 0.001f,
               "the new onset begins only the new source capture");

        uint64_t firstId = 0;
        uint64_t retiredId = 0;
        int firstSlot = -1;
        bank.reset();
        for (int i = 0; i < CapturedHitBank::maxCaptures + 1; ++i)
        {
            const auto started = bank.startCapture (80.0f);
            if (i == 0)
            {
                firstId = started.sourceId;
                firstSlot = bank.getSlotForSource (firstId);
            }
            if (i == CapturedHitBank::maxCaptures) retiredId = started.retiredSourceId;
        }
        check (retiredId == firstId && ! bank.isActive (firstId),
               "the seventeenth hit recycles the oldest of sixteen captures");
        check (! bank.isHandleActive (firstSlot, firstId),
               "slot handles reject a capture after that slot is recycled");

        bank.reset();
        const auto frameSource = bank.startCapture (80.0f);
        bank.processSample (0.3f, -0.6f);
        const int frameSlot = bank.getSlotForSource (frameSource.sourceId);
        float frameLeft = 0.0f;
        float frameRight = 0.0f;
        int frameLength = 0;
        bool frameRecording = false;
        check (bank.getHandleFrame (frameSlot, frameSource.sourceId, 0,
                                    frameLeft, frameRight,
                                    frameLength, frameRecording)
                   && std::abs (frameLeft - 0.3f) < 0.001f
                   && std::abs (frameRight + 0.6f) < 0.001f
                   && frameLength == 1
                   && frameRecording,
               "combined handle lookup returns a valid stereo frame and state");
    }

    std::printf ("StereoSafetyLimiter:\n");
    {
        StereoSafetyLimiter limiter;
        limiter.prepare (sr);
        float left = 4.0f;
        float right = -2.0f;
        limiter.process (left, right);
        check (std::abs (left) <= StereoSafetyLimiter::ceiling + 0.0001f
                   && std::abs (right) <= StereoSafetyLimiter::ceiling + 0.0001f,
               "linked limiter keeps both channels below the -1 dB ceiling");
        check (std::abs (left / right + 2.0f) < 0.001f,
               "linked limiter preserves the stereo channel ratio");
    }

    std::printf ("CapturedHitPlayback:\n");
    {
        const float attackStart = CapturedHitPlayback::envelope (0, 100, false, 10, 20);
        const float body = CapturedHitPlayback::envelope (40, 100, false, 10, 20);
        const float releaseEnd = CapturedHitPlayback::envelope (99, 100, false, 10, 20);
        check (attackStart == 0.0f && body == 1.0f && releaseEnd == 0.0f,
               "replay envelope starts and ends at zero with a full-level body");
        check (std::isfinite (attackStart) && std::isfinite (body)
                   && std::isfinite (releaseEnd),
               "replay envelope remains finite");

        float centreLeft = 0.0f, centreRight = 0.0f;
        CapturedHitPlayback::placeStereo (0.8f, -0.2f, 0.5f, centreLeft, centreRight);
        check (std::abs (centreLeft - 0.8f) < 0.0001f
                   && std::abs (centreRight + 0.2f) < 0.0001f,
               "centre particles preserve the captured stereo image");

        float edgeLeft = 0.0f, edgeRight = 0.0f;
        CapturedHitPlayback::placeStereo (0.8f, -0.2f, 0.0f, edgeLeft, edgeRight);
        check (std::abs (edgeRight) < 0.0001f && edgeLeft > 0.0f,
               "edge particles narrow the image and place it to one side");
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
        constexpr uint64_t sourceId = 101;
        ps.triggerBurst (8, 0.5f, sourceId);

        int totalEchoes = 0;
        float minPan = 1.0f, maxPan = 0.0f;
        float minElapsed = 1.0e9f, maxElapsed = -1.0e9f;
        bool gainInRange = true;
        bool sourceMatches = true;

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
                minElapsed = juce::jmin (minElapsed, e.elapsedMs);
                maxElapsed = juce::jmax (maxElapsed, e.elapsedMs);
                if (e.gain < 0.0f || e.gain > 1.0f) gainInRange = false;
                if (e.sourceId != sourceId) sourceMatches = false;
            }
        }

        check (totalEchoes > 0, "bouncing particles generate echoes");
        check (gainInRange, "all echo gains stay within [0, 1]");
        check (minPan >= 0.0f && maxPan <= 1.0f, "pan stays within [0, 1]");
        check (minElapsed >= 60.0f - 0.01f && maxElapsed <= 650.0f + 0.01f,
               "audible events stay within the Delay Min/Max bounce window");
        check (maxElapsed - minElapsed > 1.0f,
               "audible bounces retain their true elapsed time");
        check (sourceMatches, "every bounce keeps the source capture ID");

        // Particles must eventually all die (energy decay / age cap).
        ParticleSystem::ParticleSnapshot snap[ParticleSystem::maxParticles];
        const int aliveAfter = ps.getSnapshot (snap, ParticleSystem::maxParticles);
        check (aliveAfter == 0, "all particles eventually die");

        std::printf ("    (generated %d echoes; delay range %.0f..%.0f ms)\n",
                     totalEchoes, minElapsed, maxElapsed);
    }

    std::printf ("Overlapping bursts:\n");
    {
        constexpr double controlRate = 250.0;
        ParticleSystem ps;
        ps.prepare (sr, controlRate);
        std::vector<EchoEvent> events;
        events.reserve (ParticleSystem::maxParticles * 2 + 16);

        ps.triggerBurst (1, 0.0f, 11);
        for (int tick = 0; tick < 20; ++tick)
        {
            events.clear();
            ps.update (0.00035f, 0.72f, 0.9999f, 0.0f, 2000.0f, events);
        }
        ps.triggerBurst (1, 0.0f, 22);

        bool sawFirst = false;
        bool sawSecond = false;
        for (int tick = 0; tick < 1000 && (! sawFirst || ! sawSecond); ++tick)
        {
            events.clear();
            ps.update (0.00035f, 0.72f, 0.9999f, 0.0f, 2000.0f, events);
            for (const auto& e : events)
            {
                sawFirst  = sawFirst  || e.sourceId == 11;
                sawSecond = sawSecond || e.sourceId == 22;
            }
        }
        check (sawFirst && sawSecond,
               "overlapping bursts retain independent source ownership");

        ps.retireSource (11);
        bool retiredSourceReturned = false;
        for (int tick = 0; tick < 500; ++tick)
        {
            events.clear();
            ps.update (0.00035f, 0.72f, 0.9999f, 0.0f, 2000.0f, events);
            for (const auto& e : events)
                retiredSourceReturned = retiredSourceReturned || e.sourceId == 11;
        }
        check (! retiredSourceReturned,
               "retiring a recycled capture removes its queued and live particles");
    }

    std::printf ("Free gravity:\n");
    {
        constexpr double controlRate = 250.0;

        const auto bounceTimes = [] (float gravity)
        {
            ParticleSystem particles;
            particles.prepare (44100.0, controlRate);

            std::vector<EchoEvent> events;
            events.reserve (ParticleSystem::maxParticles * 2 + 16);
            particles.triggerBurst (1, 0.0f, 77);

            std::array<float, 2> times { -1.0f, -1.0f };
            for (int tick = 0; tick < 5000 && times[1] < 0.0f; ++tick)
            {
                events.clear();
                particles.update (gravity, 0.72f, 0.9999f,
                                  0.0f, 20000.0f, events);
                for (const auto& event : events)
                {
                    if (times[0] < 0.0f)
                        times[0] = event.elapsedMs;
                    else if (times[1] < 0.0f)
                        times[1] = event.elapsedMs;
                }
            }

            return times;
        };

        const float lowGravity = ParticleSystem::gravityForMultiplier (
            ParticleSystem::minimumGravityMultiplier,
            controlRate, ParticleSystem::spawnHeight);
        const float neutralGravity = ParticleSystem::gravityForMultiplier (
            ParticleSystem::defaultGravityMultiplier,
            controlRate, ParticleSystem::spawnHeight);
        const float highGravity = ParticleSystem::gravityForMultiplier (
            ParticleSystem::maximumGravityMultiplier,
            controlRate, ParticleSystem::spawnHeight);

        const auto lowTimes = bounceTimes (lowGravity);
        const auto neutralTimes = bounceTimes (neutralGravity);
        const auto highTimes = bounceTimes (highGravity);

        check (neutralTimes[0] > 0.0f
                   && std::abs (neutralTimes[0] - 250.0f) <= 8.0f,
               "1.0x gravity reaches the first impact near 250 ms");
        check (lowTimes[0] > neutralTimes[0] && neutralTimes[0] > highTimes[0],
               "gravity advances first impact monotonically");
        check (lowTimes[1] > lowTimes[0]
                   && neutralTimes[1] > neutralTimes[0]
                   && highTimes[1] > highTimes[0],
               "later impacts remain ordered across the gravity range");
        check ((lowTimes[1] - lowTimes[0]) > (highTimes[1] - highTimes[0]),
               "lower gravity produces wider bounce spacing");

        std::printf ("    (first impacts %.0f, %.0f, %.0f ms at 0.0625x, 1x, 16x)\n",
                     lowTimes[0], neutralTimes[0], highTimes[0]);
    }

    std::printf ("Bounce window:\n");
    {
        constexpr double controlRate = 250.0;
        ParticleSystem ps;
        ps.prepare (sr, controlRate);
        std::vector<EchoEvent> events;
        events.reserve (32);

        const float g = ParticleSystem::gravityForFallTime (
            0.25, controlRate, ParticleSystem::spawnHeight);
        ps.triggerBurst (1, 0.0f, 88);

        float firstAudibleElapsed = -1.0f;
        for (int tick = 0; tick < 1000 && firstAudibleElapsed < 0.0f; ++tick)
        {
            events.clear();
            ps.update (g, 0.72f, 0.9999f, 500.0f, 1000.0f, events);
            if (! events.empty())
                firstAudibleElapsed = events.front().elapsedMs;
        }
        check (firstAudibleElapsed >= 500.0f && firstAudibleElapsed <= 1000.0f,
               "bounces remain silent until they enter the audible time window");
    }

    std::printf ("Parameter compatibility:\n");
    {
        ParticleDelayAudioProcessor processor;
        auto legacyState = processor.apvts.copyState();
        legacyState.removeProperty ("GRAVITY", nullptr);

        juce::MemoryBlock legacyData;
        if (auto xml = legacyState.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, legacyData);

        processor.apvts.getRawParameterValue ("GRAVITY")->store (4.0f);
        processor.setStateInformation (legacyData.getData(),
                                       (int) legacyData.getSize());

        check (processor.apvts.getRawParameterValue ("SYNC") != nullptr,
               "legacy Sync remains registered for old sessions and automation");
        check (std::abs (processor.apvts.getRawParameterValue ("GRAVITY")->load()
                        - ParticleSystem::defaultGravityMultiplier) < 0.0001f,
               "states without Gravity load at the 1.0x default");
    }

    std::printf ("Processor high-rate stress:\n");
    {
        constexpr double stressRate = 95996.0;
        constexpr int blockSize = 32;
        constexpr double durationSeconds = 6.0;
        const int totalSamples = (int) std::round (stressRate * durationSeconds);
        const int hitInterval = (int) std::round (stressRate * 0.35);

        ParticleDelayAudioProcessor processor;
        processor.setPlayConfigDetails (2, 2, stressRate, blockSize);
        processor.prepareToPlay (stressRate, blockSize);

        processor.apvts.getRawParameterValue ("MIX")->store (1.0f);
        processor.apvts.getRawParameterValue ("PARTICLES")->store (32.0f);
        check (processor.apvts.getRawParameterValue ("SYNC") != nullptr
                   && processor.apvts.getRawParameterValue ("GRAVITY") != nullptr,
               "legacy Sync and persisted Gravity parameters are both available");
        processor.apvts.getRawParameterValue ("SYNC")->store (6.0f);
        processor.apvts.getRawParameterValue ("GRAVITY")->store (
            ParticleSystem::maximumGravityMultiplier);
        processor.apvts.getRawParameterValue ("BOUNCE")->store (0.99f);
        processor.apvts.getRawParameterValue ("SCATTER")->store (0.0f);
        processor.apvts.getRawParameterValue ("DECAY")->store (0.9999f);
        processor.apvts.getRawParameterValue ("CAPTURE_MAX_MS")->store (500.0f);
        processor.apvts.getRawParameterValue ("SMOOTHNESS")->store (0.5f);
        processor.apvts.getRawParameterValue ("DELAY_MIN_MS")->store (1.0f);
        processor.apvts.getRawParameterValue ("DELAY_MAX_MS")->store (12000.0f);
        processor.apvts.getRawParameterValue ("THRESHOLD")->store (0.1f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        bool allFinite = true;
        int renderedSamples = 0;
        int maximumVoices = 0;
        double longestBlockSeconds = 0.0;
        const auto startedAt = std::chrono::steady_clock::now();

        while (renderedSamples < totalSamples)
        {
            buffer.clear();
            const int samplesThisBlock = juce::jmin (blockSize,
                                                     totalSamples - renderedSamples);
            buffer.setSize (2, samplesThisBlock, false, false, true);

            for (int i = 0; i < samplesThisBlock; ++i)
            {
                const int timelineSample = renderedSamples + i;
                if (timelineSample % hitInterval == 0)
                {
                    buffer.setSample (0, i, 0.9f);
                    buffer.setSample (1, i, -0.45f);
                }
            }

            if (renderedSamples == blockSize * 2)
                buffer.setSample (0, 0, std::numeric_limits<float>::quiet_NaN());

            const auto blockStartedAt = std::chrono::steady_clock::now();
            processor.processBlock (buffer, midi);
            longestBlockSeconds = juce::jmax (
                longestBlockSeconds,
                std::chrono::duration<double> (
                    std::chrono::steady_clock::now() - blockStartedAt).count());
            maximumVoices = juce::jmax (
                maximumVoices,
                processor.getActiveReplayVoiceCountForTests());
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    allFinite = allFinite
                        && std::isfinite (buffer.getSample (channel, i));

            renderedSamples += samplesThisBlock;
        }

        const double elapsedSeconds =
            std::chrono::duration<double> (
                std::chrono::steady_clock::now() - startedAt).count();
        processor.releaseResources();

        check (allFinite,
               "95,996 Hz / 32-sample rendering contains invalid input and stays finite");
        check (maximumVoices >= 128,
               "high-rate stress reaches a dense replay load");
       #if JUCE_DEBUG
        check (elapsedSeconds < durationSeconds * 3.0,
               "Debug high-rate stress completes within a bounded runtime");
       #else
        check (elapsedSeconds < durationSeconds,
               "dense 95,996 Hz / 32-sample rendering completes faster than real time");
       #endif
        std::printf ("    (rendered %.1f seconds in %.3f seconds, %.1fx real time;"
                     " %d voices, longest block %.3f ms)\n",
                     durationSeconds, elapsedSeconds,
                     durationSeconds / juce::jmax (0.000001, elapsedSeconds),
                     maximumVoices, longestBlockSeconds * 1000.0);
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return failures == 0 ? 0 : 1;
}
