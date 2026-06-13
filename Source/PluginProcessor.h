#pragma once

#include <JuceHeader.h>
#include <vector>

#include "DSP/DelayBuffer.h"
#include "DSP/TransientDetector.h"
#include "DSP/ParticleSystem.h"
#include "DSP/EchoEvent.h"
#include "DSP/DelaySync.h"

//==============================================================================
// Particle Delay
//
// A physics-inspired delay. Input transients release virtual particles from the
// centre of a 2D box; gravity drops them onto the floor and they bounce there,
// losing energy each time. Every floor bounce fires a short echo "grain" read
// from a delay line. The particle's state at the moment of the bounce decides
// where, how loud, how far back, and how bright each echo is:
//
//   x position   -> pan          time since hit -> delay time
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
    double getTailLengthSeconds() const override { return maxDelayMs * 0.001 + 0.1; }

    //==========================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    // Used by the editor's visualiser (safe to call from the message thread).
    int getParticleSnapshot (ParticleSystem::ParticleSnapshot* dest, int maxOut) const
    {
        return particleSystem.getSnapshot (dest, maxOut);
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==========================================================================
    // A live echo: a windowed grain that reads a fragment from the delay line,
    // tone-shaped by a one-pole low-pass and placed in the stereo field.
    struct EchoGrain
    {
        float delayMs   = 100.0f;
        float gain      = 1.0f;
        float leftGain  = 0.707f;   // equal-power pan, precomputed at trigger
        float rightGain = 0.707f;

        int currentSample = 0;
        int totalSamples  = 1;

        float lpCoeff = 1.0f;       // brightness low-pass (one-pole)
        float lpState = 0.0f;

        bool alive = true;
    };

    void triggerGrain (const EchoEvent& e);

    //==========================================================================
    DelayBuffer       delayBuffer;
    TransientDetector transientDetector;
    ParticleSystem    particleSystem;

    std::vector<EchoEvent> echoEvents;   // refilled each control tick
    std::vector<EchoGrain> activeGrains;

    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> outputSmoothed;

    // Fixed control rate for the particle physics. Lower = slower, more spaced-out
    // scatter and bounces (everything in the sim is per-tick, so this scales it all).
    static constexpr double controlRateHz = 250.0;
    double samplesPerControlTick = 192.0;
    double controlAccumulator    = 0.0;

    int grainLengthSamples = 1;

    static constexpr int maxGrains   = 256;
    static constexpr float grainMs   = 45.0f;
    static constexpr float wetSafety = 0.9f;   // headroom before the wet soft-clip
    static constexpr float maxDelayMs = 12000.0f;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleDelayAudioProcessor)
};
