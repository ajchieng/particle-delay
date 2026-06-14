#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ParticleLookAndFeel.h"
#include <array>
#include <vector>
#include <memory>

//==============================================================================
// Live 2D view of the particle box. On a timer it pulls a snapshot of particle
// positions from the processor (lock-free) and draws each as a glowing dot whose
// size/opacity follow its remaining energy. Left/right maps to stereo placement.
class ParticleView : public juce::Component,
                     private juce::Timer
{
public:
    explicit ParticleView (ParticleDelayAudioProcessor&);
    ~ParticleView() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    // Renders the current particle set into trailBuffer over the faded previous
    // frame, producing motion trails without needing stable particle ids.
    void updateTrail();
    void paintBoxBackground (juce::Graphics&, juce::Rectangle<float> area, float alpha);

    ParticleDelayAudioProcessor& proc;

    static constexpr int maxDots = ParticleSystem::maxParticles;
    std::array<ParticleSystem::ParticleSnapshot, maxDots> dots {};
    int numDots = 0;

    juce::Image trailBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleView)
};

//==============================================================================
// A thin horizontal meter under the Threshold knob: a fill bar tracks the live
// input peak and a marker line shows the Threshold, both placed on the Threshold
// parameter's own (skewed) scale so the bar crosses the marker exactly when a
// hit will trigger.
class MeterView : public juce::Component,
                  private juce::Timer
{
public:
    explicit MeterView (ParticleDelayAudioProcessor&);
    ~MeterView() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    ParticleDelayAudioProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterView)
};

//==============================================================================
// A toggle-able overlay that explains the plugin and every parameter. The text
// mirrors PARAMETER_GUIDE.md, condensed to one line per control. Hidden until
// the header "?" button shows it; closes via its button or a backdrop click.
class HelpPanel : public juce::Component
{
public:
    HelpPanel();
    ~HelpPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // Set by the editor to hide the panel.
    std::function<void()> onClose;

private:
    // Scrollable rich-text body painted from a single AttributedString.
    class Body : public juce::Component
    {
    public:
        Body();
        void paint (juce::Graphics&) override;
        int idealHeightForWidth (int width) const;

    private:
        juce::AttributedString text;
    };

    juce::Rectangle<int> cardBounds() const;

    juce::Viewport   viewport;
    Body             body;
    juce::TextButton closeButton { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpPanel)
};

//==============================================================================
// 18 rotary controls, two delay-sync sections, the particle visualiser, and a
// toggle-able help overlay.
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

    // A labelled card grouping a contiguous run of knobs.
    struct Section
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    void addKnob (Knob& knob, const juce::String& paramID,
                  const juce::String& displayName, const juce::String& tooltip);
    void addDelaySyncControl (DelaySyncControl& control,
                              const juce::String& syncParamID,
                              const juce::String& divisionParamID);
    // Lay out 'count' knobs in a single row across 'inner', starting at knob index.
    void layoutKnobRow (juce::Rectangle<int> inner, int startIndex, int count);

    // Preset bar helpers.
    void refreshPresetList();          // repopulate the combo from the PresetManager
    void applySelectedPreset();        // load whatever the combo currently shows
    void stepPreset (int delta);       // < / > buttons
    void promptSaveUserPreset();       // "Save" button

    // Declared first so it outlives the child controls that borrow it; the
    // destructor also clears it via setLookAndFeel (nullptr).
    ParticleLookAndFeel lookAndFeel;

    // Shows tooltips for any child that calls setTooltip; declared after the
    // look-and-feel so it is destroyed first.
    juce::TooltipWindow tooltip;

    ParticleDelayAudioProcessor& proc;

    juce::Label      titleLabel;
    juce::TextButton resetButton { "Reset" };
    juce::TextButton helpButton { "?" };
    ParticleView     particleView;
    MeterView        thresholdMeter;
    HelpPanel        helpPanel;

    // Preset bar.
    juce::ComboBox   presetBox;
    juce::TextButton prevPresetButton { "<" };
    juce::TextButton nextPresetButton { ">" };
    juce::TextButton savePresetButton { "Save" };
    juce::StringArray userPresetNames;          // index aligns with combo ids >= userPresetIdBase
    static constexpr int userPresetIdBase = 1000;

    static constexpr int numKnobs = 18;
    std::array<Knob, numKnobs> knobs;
    std::array<DelaySyncControl, 2> delaySyncControls;
    std::array<Section, 5> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleDelayAudioProcessorEditor)
};
