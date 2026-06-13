#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <vector>
#include <memory>

//==============================================================================
// Live 2D view of the particle box. On a timer it pulls a snapshot of particle
// positions from the processor (lock-free) and draws each as a glowing dot whose
// size/opacity follow its remaining energy. Left/right maps to pan, top/bottom
// to delay time - so you literally watch the echoes being scattered.
class ParticleView : public juce::Component,
                     private juce::Timer
{
public:
    explicit ParticleView (ParticleDelayAudioProcessor&);
    ~ParticleView() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    ParticleDelayAudioProcessor& proc;

    static constexpr int maxDots = ParticleSystem::maxParticles;
    std::array<ParticleSystem::ParticleSnapshot, maxDots> dots {};
    int numDots = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleView)
};

//==============================================================================
// 10 rotary controls, two delay-sync sections, and the particle visualiser.
class ParticleDelayAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ParticleDelayAudioProcessorEditor (ParticleDelayAudioProcessor&);
    ~ParticleDelayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct DelaySyncControl
    {
        juce::ToggleButton syncButton;
        juce::ComboBox divisionBox;
        std::unique_ptr<ButtonAttachment> buttonAttachment;
        std::unique_ptr<ComboBoxAttachment> divisionAttachment;
    };

    void addKnob (Knob& knob, const juce::String& paramID, const juce::String& displayName);
    void addDelaySyncControl (DelaySyncControl& control,
                              const juce::String& syncParamID,
                              const juce::String& divisionParamID);

    ParticleDelayAudioProcessor& proc;

    juce::Label   titleLabel;
    ParticleView  particleView;

    static constexpr int numKnobs = 10;
    std::array<Knob, numKnobs> knobs;
    std::array<DelaySyncControl, 2> delaySyncControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleDelayAudioProcessorEditor)
};
