#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

#include "DSP/CapturedHit.h"
#include "DSP/TransientDetector.h"
#include "DSP/ParticleSystem.h"
#include "DSP/EchoEvent.h"
#include "DSP/DelaySync.h"
#include "DSP/WetFinisher.h"
#include "PresetManager.h"

//==============================================================================
// Particle Delay
//
// A physics-inspired delay. Input transients release virtual particles from the
// centre of a 2D box; gravity drops them onto the floor and they bounce there,
// losing energy each time. Every floor bounce replays the stereo transient that
// spawned that particle. The particle state decides where, how loud, and how
// bright each replay is:
//
//   x position   -> stereo pan   elapsed time -> audible bounce window
//   energy       -> gain         impact speed -> brightness (low-pass cutoff)
//
// The particle physics runs at a fixed 250 Hz control rate, independent of the
// host sample rate. The default gravity/scatter/decay values are tuned for that
// tick rate (~4 ms), which is what spaces the echoes out into musical taps
// rather than an audio-rate buzz. (Velocity, gravity and decay are all per-tick,
// so the control rate is the single knob that scales the whole sim in wall time.)
class ParticleDelayAudioProcessor : public juce::AudioProcessor
{
public:
    ParticleDelayAudioProcessor();
    ~ParticleDelayAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // Cover the worst case: max Feedback stretches a particle's life to ~20 s
    // (see ParticleSystem effMaxAge), matching the maximum delay window.
    double getTailLengthSeconds() const override { return juce::jmax (maxDelayMs * 0.001, 20.0) + 0.2; }

    //==========================================================================
    // Programs are the factory presets, surfaced in the host's preset menu.
    int getNumPrograms() override { return presetManager.getNumFactoryPresets(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override
    {
        if (index >= 0 && index < presetManager.getNumFactoryPresets())
        {
            currentProgram = index;
            presetManager.applyFactoryPreset (index);
        }
    }
    const juce::String getProgramName (int index) override { return presetManager.getFactoryPresetName (index); }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Reset every parameter to its default value (the editor's Reset button).
    void resetToDefaults();

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    // Factory + user preset handling (used by the editor's preset bar and the
    // host program interface). Declared after apvts so its reference is valid.
    PresetManager presetManager { *this, apvts };

    // Used by the editor's visualiser (safe to call from the message thread).
    int getParticleSnapshot (ParticleSystem::ParticleSnapshot* dest, int maxOut) const
    {
        return particleSystem.getSnapshot (dest, maxOut);
    }

    // Latest input peak (0..1), for the editor's threshold meter. Lock-free.
    float getInputLevel() const noexcept
    {
        return inputLevel.load (std::memory_order_relaxed);
    }

   #if defined (PARTICLEDELAY_ENABLE_TEST_HOOKS)
    int getActiveReplayVoiceCountForTests() const noexcept
    {
        return activeVoiceCount;
    }

    float getOverlapGainForTests() const noexcept
    {
        return overlapGainSmoothed.getCurrentValue();
    }
   #endif

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==========================================================================
    // A live replay of one captured stereo transient.
    struct ReplayVoice
    {
        uint64_t sourceId = 0;
        int captureSlot = -1;
        float gain = 1.0f;
        float pan = 0.5f;
        float lpCoeff = 1.0f;
        float lpStateLeft = 0.0f;
        float lpStateRight = 0.0f;
        float lastLevel = 0.0f;
        int currentSample = 0;
        int attackSamples = 1;
        int releaseSamples = 1;
    };

    void triggerReplay (const EchoEvent& e, float smoothness);
    void retireSource (uint64_t sourceId);
    void removeReplayVoice (int index);

    //==========================================================================
    CapturedHitBank   capturedHits;
    StereoSafetyLimiter wetLimiter;
    WetFinisher       wetFinisher;
    TransientDetector transientDetector;
    ParticleSystem    particleSystem;

    int currentProgram = 0;

    std::vector<EchoEvent> echoEvents;   // refilled each control tick
    static constexpr int maxReplayVoices = 256;
    std::array<ReplayVoice, maxReplayVoices> activeVoices;
    int activeVoiceCount = 0;

    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> outputSmoothed;

    // Slews the wet overlap-normalisation gain so the level doesn't jump when a
    // replay voice starts or ends.
    juce::SmoothedValue<float> overlapGainSmoothed;

    // Decaying input peak published for the editor's threshold meter.
    std::atomic<float> inputLevel { 0.0f };
    float inputLevelEnv = 0.0f;

    // Fixed control rate for the particle physics. Lower = slower, more spaced-out
    // scatter and bounces (everything in the sim is per-tick, so this scales it all).
    static constexpr double controlRateHz = 250.0;
    double samplesPerControlTick = 192.0;
    double controlAccumulator    = 0.0;

    static constexpr float maxDelayMs = 20000.0f;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleDelayAudioProcessor)
};
