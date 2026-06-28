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
#include <memory>
#include <vector>

#include "../Source/PluginProcessor.h"
#include "../Source/DSP/DelayBuffer.h"
#include "../Source/DSP/TransientDetector.h"
#include "../Source/DSP/ParticleSystem.h"
#include "../Source/DSP/EchoEvent.h"
#include "../Source/DSP/DelaySync.h"
#include "../Source/DSP/CapturedHit.h"
#include "../Source/DSP/WetFinisher.h"
#include "../Source/PresetManager.h"

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", condition ? "PASS" : "FAIL", what.toRawUTF8());
        if (! condition)
            ++failures;
    }

    void inspectLabels (juce::Component& component, int& labelCount, int& darkLabelCount)
    {
        if (auto* label = dynamic_cast<juce::Label*> (&component))
        {
            const auto colour = label->findColour (juce::Label::textColourId);
            ++labelCount;

            if (colour.getPerceivedBrightness() < 0.35f)
                ++darkLabelCount;
        }

        for (int i = 0; i < component.getNumChildComponents(); ++i)
            inspectLabels (*component.getChildComponent (i), labelCount, darkLabelCount);
    }

    void inspectDoubleClickEditableSliders (juce::Component& component,
                                            int& sliderCount,
                                            int& editableSliderCount)
    {
        if (auto* slider = dynamic_cast<juce::Slider*> (&component))
        {
            ++sliderCount;

            if ((bool) slider->getProperties()["opensTextEditorOnDoubleClick"])
                ++editableSliderCount;
        }

        for (int i = 0; i < component.getNumChildComponents(); ++i)
            inspectDoubleClickEditableSliders (*component.getChildComponent (i),
                                               sliderCount,
                                               editableSliderCount);
    }

    juce::Button* findButton (juce::Component& component, const juce::String& text)
    {
        if (auto* button = dynamic_cast<juce::Button*> (&component);
            button != nullptr && button->getButtonText() == text)
            return button;

        for (int i = 0; i < component.getNumChildComponents(); ++i)
            if (auto* button = findButton (*component.getChildComponent (i), text))
                return button;

        return nullptr;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    constexpr double sr = 48000.0;

    std::printf ("Editor colours:\n");
    {
        ParticleDelayAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        int labelCount = 0;
        int darkLabelCount = 0;
        int sliderCount = 0;
        int editableSliderCount = 0;
        inspectLabels (*editor, labelCount, darkLabelCount);
        inspectDoubleClickEditableSliders (*editor, sliderCount, editableSliderCount);

        check (editor->getWidth() == 900 && editor->getHeight() == 574,
               "editor uses the default 900x574 frame");
        check (labelCount >= 38, "editor exposes all control labels");
        check (darkLabelCount == 0, "all rendered label text resolves to a light colour");
        check (sliderCount == 18 && editableSliderCount == sliderCount,
               "all rotary knobs open text entry on double-click");

        auto* buttonA = findButton (*editor, "A");
        auto* buttonB = findButton (*editor, "B");
        auto* bypass = findButton (*editor, "BYPASS");
        check (buttonA != nullptr && buttonB != nullptr && bypass != nullptr,
               "editor exposes the A/B and bypass buttons");

        if (buttonA != nullptr && buttonB != nullptr && bypass != nullptr)
        {
            auto* mix = processor.apvts.getParameter ("MIX");
            mix->setValueNotifyingHost (0.2f);
            buttonB->onClick();
            mix->setValueNotifyingHost (0.8f);
            buttonA->onClick();
            check (std::abs (mix->getValue() - 0.2f) < 0.001f,
                   "A/B buttons recall independent parameter states");

            bypass->setToggleState (true, juce::sendNotificationSync);
            check (processor.apvts.getRawParameterValue ("BYPASS")->load() >= 0.5f,
                   "bypass button updates the processor parameter");
        }
    }

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

    std::printf ("Parameter ranges:\n");
    {
        ParticleDelayAudioProcessor processor;
        const auto delayMinRange = processor.apvts.getParameterRange ("DELAY_MIN_MS");
        check (std::abs (delayMinRange.convertFrom0to1 (0.5f) - 250.0f) < 1.0f,
               "Delay Min knob midpoint is less heavily skewed");
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
            ps.update (0.00035f, 0.72f, 0.995f, 0.0f, 60.0f, 650.0f, events);

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
            ps.update (0.00035f, 0.72f, 0.9999f, 0.0f, 0.0f, 2000.0f, events);
        }
        ps.triggerBurst (1, 0.0f, 22);

        bool sawFirst = false;
        bool sawSecond = false;
        for (int tick = 0; tick < 1000 && (! sawFirst || ! sawSecond); ++tick)
        {
            events.clear();
            ps.update (0.00035f, 0.72f, 0.9999f, 0.0f, 0.0f, 2000.0f, events);
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
            ps.update (0.00035f, 0.72f, 0.9999f, 0.0f, 0.0f, 2000.0f, events);
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
                                  0.0f, 0.0f, 20000.0f, events);
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

    std::printf ("Timing modes:\n");
    {
        constexpr double controlRate = 250.0;
        const float freeGravity = ParticleSystem::gravityForTimingMode (
            0, 9, 4.0f, 120.0, controlRate, ParticleSystem::spawnHeight);
        const float existingFreeGravity = ParticleSystem::gravityForMultiplier (
            4.0f, controlRate, ParticleSystem::spawnHeight);
        check (std::abs (freeGravity - existingFreeGravity) < 1.0e-8f,
               "Free timing mode preserves the existing Gravity behavior");

        const float tempoGravity = ParticleSystem::gravityForTimingMode (
            1, 9, 16.0f, 120.0, controlRate, ParticleSystem::spawnHeight);
        const float expectedQuarterGravity = ParticleSystem::gravityForFallTime (
            0.5, controlRate, ParticleSystem::spawnHeight);
        check (std::abs (tempoGravity - expectedQuarterGravity) < 1.0e-8f,
               "Tempo timing mode targets the selected note division and ignores Gravity");

        const float hybridLow = ParticleSystem::gravityForTimingMode (
            2, 9, 0.25f, 120.0, controlRate, ParticleSystem::spawnHeight);
        const float hybridHigh = ParticleSystem::gravityForTimingMode (
            2, 9, 4.0f, 120.0, controlRate, ParticleSystem::spawnHeight);
        check (hybridLow < expectedQuarterGravity
                   && hybridHigh > expectedQuarterGravity
                   && hybridHigh < existingFreeGravity,
               "Hybrid timing mode syncs the centre while Gravity offsets the feel");
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
            ps.update (g, 0.72f, 0.9999f, 0.0f, 500.0f, 1000.0f, events);
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
        check (processor.apvts.getRawParameterValue ("TIMING_MODE") != nullptr
                   && processor.apvts.getRawParameterValue ("TIMING_DIV") != nullptr,
               "timing mode parameters are available for new sessions and automation");
        check (processor.apvts.getRawParameterValue ("TIMING_MODE")->load() < 0.5f,
               "Timing Mode defaults to Free for old-session compatibility");
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
        check (processor.apvts.getRawParameterValue ("TIMING_MODE") != nullptr
                   && processor.apvts.getRawParameterValue ("TIMING_DIV") != nullptr,
               "timing mode parameters are available during processor rendering");
        processor.apvts.getRawParameterValue ("SYNC")->store (6.0f);
        processor.apvts.getRawParameterValue ("GRAVITY")->store (
            ParticleSystem::maximumGravityMultiplier);
        processor.apvts.getRawParameterValue ("BOUNCE")->store (0.99f);
        processor.apvts.getRawParameterValue ("SCATTER")->store (0.0f);
        processor.apvts.getRawParameterValue ("DECAY")->store (0.9999f);
        processor.apvts.getRawParameterValue ("CAPTURE_MAX_MS")->store (500.0f);
        processor.apvts.getRawParameterValue ("SMOOTHNESS")->store (0.5f);
        processor.apvts.getRawParameterValue ("DELAY_MIN_MS")->store (1.0f);
        processor.apvts.getRawParameterValue ("DELAY_MAX_MS")->store (20000.0f);
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

    std::printf ("WetFinisher:\n");
    {
        constexpr int n = 4096;

        auto makeSine = [&] (float freq, std::vector<float>& l, std::vector<float>& r)
        {
            l.resize ((size_t) n);
            r.resize ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const float s = std::sin (2.0f * juce::MathConstants<float>::pi
                                          * freq * (float) i / (float) sr);
                l[(size_t) i] = s;
                r[(size_t) i] = s;
            }
        };

        // RMS over the back half of the buffer, so the filter warm-up is ignored.
        auto processRms = [] (WetFinisher& wf, std::vector<float> l, std::vector<float> r)
        {
            double sum = 0.0;
            int counted = 0;
            for (size_t i = 0; i < l.size(); ++i)
            {
                float a = l[i], b = r[i];
                wf.process (a, b);
                if ((int) i >= (int) l.size() / 2)
                {
                    sum += 0.5 * ((double) a * a + (double) b * b);
                    ++counted;
                }
            }
            return std::sqrt (sum / (double) juce::jmax (1, counted));
        };

        auto configure = [] (WetFinisher& wf, float hp, float lp, float diffuse, float width)
        {
            wf.setHighPass (hp);
            wf.setLowPass (lp);
            wf.setDiffuse (diffuse, 0.6f);
            wf.setWidth (width);
        };

        // 1) Width = 0 collapses the wet bus to mono regardless of filtering.
        {
            WetFinisher wf; wf.prepare (sr);
            configure (wf, 20.0f, 20000.0f, 0.0f, 0.0f);
            bool mono = true;
            for (int i = 0; i < 256; ++i)
            {
                float a = 0.5f, b = -0.3f;
                wf.process (a, b);
                if (std::abs (a - b) > 1.0e-6f) mono = false;
            }
            check (mono, "width = 0 collapses the wet bus to mono (L == R)");
        }

        // 2) Low-pass attenuates a high tone well below a low tone.
        {
            std::vector<float> ll, lr, hl, hr;
            makeSine (200.0f, ll, lr);
            makeSine (10000.0f, hl, hr);
            WetFinisher lowWf;  lowWf.prepare (sr);  configure (lowWf,  20.0f, 1000.0f, 0.0f, 1.0f);
            WetFinisher highWf; highWf.prepare (sr); configure (highWf, 20.0f, 1000.0f, 0.0f, 1.0f);
            const double lowRms  = processRms (lowWf,  ll, lr);
            const double highRms = processRms (highWf, hl, hr);
            check (highRms < lowRms * 0.5,
                   "low-pass attenuates a 10 kHz tone well below a 200 Hz tone");
        }

        // 3) High-pass attenuates a low tone well below a high tone.
        {
            std::vector<float> ll, lr, hl, hr;
            makeSine (80.0f, ll, lr);
            makeSine (5000.0f, hl, hr);
            WetFinisher lowWf;  lowWf.prepare (sr);  configure (lowWf,  2000.0f, 20000.0f, 0.0f, 1.0f);
            WetFinisher highWf; highWf.prepare (sr); configure (highWf, 2000.0f, 20000.0f, 0.0f, 1.0f);
            const double lowRms  = processRms (lowWf,  ll, lr);
            const double highRms = processRms (highWf, hl, hr);
            check (lowRms < highRms * 0.5,
                   "high-pass attenuates an 80 Hz tone well below a 5 kHz tone");
        }

        // 4) Diffuse alters the signal and stays finite/bounded.
        {
            std::vector<float> dl, dr;
            makeSine (1000.0f, dl, dr);
            WetFinisher wf; wf.prepare (sr);
            configure (wf, 20.0f, 20000.0f, 1.0f, 1.0f);
            bool finite = true, bounded = true, changed = false;
            for (size_t i = 0; i < dl.size(); ++i)
            {
                const float inL = dl[i];
                float a = dl[i], b = dr[i];
                wf.process (a, b);
                finite  = finite  && std::isfinite (a) && std::isfinite (b);
                bounded = bounded && std::abs (a) < 8.0f && std::abs (b) < 8.0f;
                if (std::abs (a - inL) > 1.0e-4f) changed = true;
            }
            check (finite && bounded, "diffuser output stays finite and bounded");
            check (changed, "diffuse > 0 audibly alters the wet signal");
        }

        // 5) Default settings are ~transparent.
        {
            std::vector<float> il, ir;
            makeSine (1000.0f, il, ir);
            WetFinisher wf; wf.prepare (sr);
            configure (wf, 20.0f, 20000.0f, 0.0f, 1.0f);
            double sumIn = 0.0, sumOut = 0.0;
            for (size_t i = 0; i < il.size(); ++i)
            {
                const float inL = il[i];
                float a = il[i], b = ir[i];
                wf.process (a, b);
                if ((int) i >= n / 2)
                {
                    sumIn  += (double) inL * inL;
                    sumOut += (double) a * a;
                }
            }
            const double ratio = std::sqrt (sumOut / juce::jmax (1.0e-12, sumIn));
            check (ratio > 0.9 && ratio < 1.1,
                   "default settings pass the wet signal through ~unchanged");
        }
    }

    std::printf ("Feedback extends life:\n");
    {
        constexpr double controlRate = 250.0;

        const auto run = [&] (float feedback, int& echoesOut, int& aliveOut)
        {
            ParticleSystem ps;
            ps.prepare (sr, controlRate);
            std::vector<EchoEvent> events;
            events.reserve (ParticleSystem::maxParticles * 2 + 16);
            ps.triggerBurst (4, 0.3f, 55);

            // Long enough that even the feedback-stretched lifetime cap expires.
            echoesOut = 0;
            for (int tick = 0; tick < 12000; ++tick)
            {
                events.clear();
                ps.update (0.00035f, 0.72f, 0.995f, feedback, 0.0f, 20000.0f, events);
                echoesOut += (int) events.size();
            }
            ParticleSystem::ParticleSnapshot snap[ParticleSystem::maxParticles];
            aliveOut = ps.getSnapshot (snap, ParticleSystem::maxParticles);
        };

        int dryEchoes = 0, dryAlive = 0, wetEchoes = 0, wetAlive = 0;
        run (0.0f, dryEchoes, dryAlive);
        run (0.9f, wetEchoes, wetAlive);

        check (wetEchoes > dryEchoes,
               "feedback produces more echoes (longer-lived trains)");
        check (dryAlive == 0 && wetAlive == 0,
               "particles still all die under high feedback (bounded lifetime)");

        std::printf ("    (echoes: feedback 0 -> %d, feedback 0.9 -> %d)\n",
                     dryEchoes, wetEchoes);
    }

    std::printf ("Reset to defaults:\n");
    {
        ParticleDelayAudioProcessor processor;

        // Push parameters of several types away from their defaults.
        processor.apvts.getParameter ("MIX")->setValueNotifyingHost (1.0f);
        processor.apvts.getParameter ("FEEDBACK")->setValueNotifyingHost (1.0f);
        processor.apvts.getParameter ("GRAVITY")->setValueNotifyingHost (0.0f);
        processor.apvts.getParameter ("DELAY_MIN_SYNC")->setValueNotifyingHost (1.0f);

        check (processor.apvts.getRawParameterValue ("MIX")->load() > 0.9f
                   && processor.apvts.getRawParameterValue ("FEEDBACK")->load() > 0.9f,
               "parameters move away from their defaults before reset");

        processor.resetToDefaults();

        check (std::abs (processor.apvts.getRawParameterValue ("MIX")->load() - 0.35f) < 1.0e-4f,
               "reset restores Mix to its default");
        check (std::abs (processor.apvts.getRawParameterValue ("FEEDBACK")->load()) < 1.0e-4f,
               "reset restores Feedback to 0");
        check (std::abs (processor.apvts.getRawParameterValue ("GRAVITY")->load()
                        - ParticleSystem::defaultGravityMultiplier) < 1.0e-3f,
               "reset restores Gravity to 1.0x");
        check (processor.apvts.getRawParameterValue ("DELAY_MIN_SYNC")->load() < 0.5f,
               "reset turns Delay Min Sync back off");
    }

    std::printf ("Overlap gain smoothing:\n");
    {
        constexpr int block = 64;
        ParticleDelayAudioProcessor processor;
        processor.setPlayConfigDetails (2, 2, sr, block);
        processor.prepareToPlay (sr, block);
        processor.apvts.getParameter ("MIX")->setValueNotifyingHost (1.0f);
        processor.apvts.getParameter ("PARTICLES")->setValueNotifyingHost (1.0f); // 32
        processor.apvts.getParameter ("THRESHOLD")->setValueNotifyingHost (0.0f); // triggers easily

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;

        float prevGain = processor.getOverlapGainForTests();
        float maxJump = 0.0f;
        for (int b = 0; b < 400; ++b)
        {
            buffer.clear();
            if (b < 200 && b % 4 == 0)           // build voices, then go silent
            {
                buffer.setSample (0, 0, 0.9f);
                buffer.setSample (1, 0, 0.8f);
            }
            processor.processBlock (buffer, midi);
            const float g = processor.getOverlapGainForTests();
            maxJump = juce::jmax (maxJump, std::abs (g - prevGain));
            prevGain = g;
        }
        check (maxJump < 0.15f, "overlap gain slews instead of jumping as voices start/stop");
    }

    std::printf ("Input meter:\n");
    {
        ParticleDelayAudioProcessor processor;
        processor.setPlayConfigDetails (2, 2, sr, 128);
        processor.prepareToPlay (sr, 128);
        juce::AudioBuffer<float> buffer (2, 128);
        juce::MidiBuffer midi;

        buffer.clear();
        processor.processBlock (buffer, midi);
        const float quiet = processor.getInputLevel();

        for (int i = 0; i < 128; ++i) { buffer.setSample (0, i, 0.7f); buffer.setSample (1, i, -0.7f); }
        processor.processBlock (buffer, midi);
        const float loud = processor.getInputLevel();

        check (std::isfinite (quiet) && std::isfinite (loud), "input level stays finite");
        check (loud > quiet && loud > 0.5f, "input meter rises with input level");
    }

    std::printf ("Bypass:\n");
    {
        constexpr int block = 128;
        ParticleDelayAudioProcessor processor;
        processor.setPlayConfigDetails (2, 2, sr, block);
        processor.prepareToPlay (sr, block);
        processor.apvts.getParameter ("BYPASS")->setValueNotifyingHost (1.0f);

        juce::AudioBuffer<float> buffer (2, block);
        for (int i = 0; i < block; ++i)
        {
            buffer.setSample (0, i, std::sin ((float) i * 0.11f) * 0.5f);
            buffer.setSample (1, i, std::cos ((float) i * 0.07f) * 0.3f);
        }

        juce::AudioBuffer<float> original;
        original.makeCopyOf (buffer);

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        bool unchanged = true;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                unchanged = unchanged
                    && std::abs (buffer.getSample (channel, i)
                                 - original.getSample (channel, i)) < 1.0e-7f;

        check (unchanged, "bypass leaves the input buffer unchanged");
        check (processor.getInputLevel() > 0.2f, "bypass still updates the input meter");
    }

    std::printf ("Presets:\n");
    {
        ParticleDelayAudioProcessor processor;
        auto& pm = processor.presetManager;

        check (pm.getNumFactoryPresets() >= 16, "factory preset bank includes a practical release set");
        check (processor.getNumPrograms() == pm.getNumFactoryPresets(),
               "program count matches the factory preset bank");

        const auto indexOfPreset = [&] (const juce::String& name)
        {
            for (int i = 0; i < pm.getNumFactoryPresets(); ++i)
                if (pm.getFactoryPresetName (i) == name) return i;
            return -1;
        };

        const int chaos = indexOfPreset ("Chaos Cloud");
        check (chaos >= 0, "Chaos Cloud factory preset exists");
        processor.setCurrentProgram (chaos);
        check (std::abs (processor.apvts.getRawParameterValue ("MIX")->load() - 0.65f) < 0.02f,
               "applying a factory preset sets its parameters");
        check (std::abs (processor.apvts.getRawParameterValue ("FEEDBACK")->load()) < 1.0e-3f,
               "a factory preset resets parameters it omits (Feedback) to default");

        const int lush = indexOfPreset ("Lush");
        check (lush >= 0, "Lush factory preset exists");
        processor.setCurrentProgram (lush);
        check (processor.apvts.getRawParameterValue ("FEEDBACK")->load() > 0.1f,
               "the Lush preset engages the SPACE panel");

        const std::array<const char*, 11> practicalPresetNames {{
            "Drum Tight Slap",
            "Drum Room Scatter",
            "Snare Ghosts",
            "Vocal Doubler Cloud",
            "Vocal Throw",
            "Piano Sparkle",
            "Guitar Halo",
            "Dub Send",
            "Ambient Wash",
            "Bass Safe Space",
            "Wide Micro Scatter",
        }};

        bool allPracticalPresetsExist = true;
        for (const auto* name : practicalPresetNames)
            allPracticalPresetsExist = allPracticalPresetsExist && indexOfPreset (name) >= 0;
        check (allPracticalPresetsExist, "practical factory presets cover common source/use cases");

        const int bassSafe = indexOfPreset ("Bass Safe Space");
        if (bassSafe >= 0)
        {
            processor.setCurrentProgram (bassSafe);
            check (processor.apvts.getRawParameterValue ("WET_HP")->load() >= 160.0f,
                   "Bass Safe Space high-passes the wet signal");
        }

        const int dubSend = indexOfPreset ("Dub Send");
        if (dubSend >= 0)
        {
            processor.setCurrentProgram (dubSend);
            check (processor.apvts.getRawParameterValue ("FEEDBACK")->load() >= 0.55f
                       && processor.apvts.getRawParameterValue ("WET_LP")->load() <= 6500.0f,
                   "Dub Send uses a darker feedback-forward wet path");
        }

        // User preset round-trip (writes to the real preset dir; cleaned up after).
        const juce::String testName = "__pd_test_preset__";
        processor.apvts.getParameter ("MIX")->setValueNotifyingHost (0.5f);
        const float savedMix = processor.apvts.getRawParameterValue ("MIX")->load();
        check (pm.saveUserPreset (testName), "user preset saves to disk");

        processor.apvts.getParameter ("MIX")->setValueNotifyingHost (0.9f);
        check (pm.loadUserPreset (testName), "user preset loads from disk");
        check (std::abs (processor.apvts.getRawParameterValue ("MIX")->load() - savedMix) < 1.0e-3f,
               "user preset round-trips the parameter state");

        PresetManager::presetDirectory().getChildFile (testName + ".xml").deleteFile();
        check (pm.getUserPresetNames().indexOf (testName) < 0, "test preset cleaned up");
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return failures == 0 ? 0 : 1;
}
