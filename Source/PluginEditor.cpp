#include "PluginEditor.h"

namespace
{
    // Knob order: three rows of four.
    struct KnobDef { const char* id; const char* name; };
    const std::array<KnobDef, 12> knobDefs {{
        { "MIX",          "Mix"       },
        { "PARTICLES",    "Particles" },
        { "GRAVITY",      "Gravity"   },
        { "BOUNCE",       "Bounce"    },
        { "SCATTER",      "Scatter"   },
        { "DECAY",        "Decay"     },
        { "CAPTURE_MAX_MS", "Capture Length" },
        { "SMOOTHNESS",     "Smoothness" },
        { "DELAY_MIN_MS", "Delay Min" },
        { "DELAY_MAX_MS", "Delay Max" },
        { "THRESHOLD",    "Threshold" },
        { "OUTPUT",       "Output"    },
    }};

    const juce::Colour kBackground { 0xff10141c };
    const juce::Colour kBoxFill    { 0xff161c28 };
    const juce::Colour kAccent     { 0xff4fd0e0 };
}

//==============================================================================
ParticleView::ParticleView (ParticleDelayAudioProcessor& p) : proc (p)
{
    setOpaque (false);
    startTimerHz (30);
}

ParticleView::~ParticleView() { stopTimer(); }

void ParticleView::timerCallback()
{
    numDots = proc.getParticleSnapshot (dots.data(), maxDots);
    repaint();
}

void ParticleView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    // Box.
    g.setColour (kBoxFill);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (kAccent.withAlpha (0.35f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.5f);

    // Faint vertical centre line: the pan reference (left/right = pan).
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawVerticalLine ((int) bounds.getCentreX(), bounds.getY(), bounds.getBottom());

    // Floor: particles bounce here, and every contact fires an echo.
    g.setColour (kAccent.withAlpha (0.5f));
    g.drawHorizontalLine ((int) bounds.getBottom(), bounds.getX(), bounds.getRight());

    // Particles.
    for (int i = 0; i < numDots; ++i)
    {
        const auto& d = dots[(size_t) i];
        const float energy = juce::jlimit (0.0f, 1.0f, d.energy);

        const float cx = bounds.getX() + d.x * bounds.getWidth();
        const float cy = bounds.getY() + d.y * bounds.getHeight();
        const float r  = 2.0f + energy * 6.0f;

        g.setColour (kAccent.withAlpha (0.15f + energy * 0.4f));
        g.fillEllipse (cx - r * 1.8f, cy - r * 1.8f, r * 3.6f, r * 3.6f); // glow
        g.setColour (juce::Colours::white.withAlpha (0.4f + energy * 0.6f));
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    if (numDots == 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.setFont (14.0f);
        g.drawText ("feed me a transient", bounds, juce::Justification::centred);
    }
}

//==============================================================================
ParticleDelayAudioProcessorEditor::ParticleDelayAudioProcessorEditor (ParticleDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), particleView (p)
{
    titleLabel.setText ("PARTICLE DELAY", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, kAccent);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    addAndMakeVisible (particleView);

    for (int i = 0; i < numKnobs; ++i)
        addKnob (knobs[(size_t) i], knobDefs[(size_t) i].id, knobDefs[(size_t) i].name);

    addDelaySyncControl (delaySyncControls[0], "DELAY_MIN_SYNC", "DELAY_MIN_DIV");
    addDelaySyncControl (delaySyncControls[1], "DELAY_MAX_SYNC", "DELAY_MAX_DIV");

    setSize (780, 620);
}

ParticleDelayAudioProcessorEditor::~ParticleDelayAudioProcessorEditor() = default;

void ParticleDelayAudioProcessorEditor::addKnob (Knob& knob,
                                                 const juce::String& paramID,
                                                 const juce::String& displayName)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    knob.slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    knob.slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    addAndMakeVisible (knob.slider);

    knob.label.setText (displayName, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
    knob.label.setFont (juce::Font (juce::FontOptions (13.0f)));
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramID, knob.slider);
}

void ParticleDelayAudioProcessorEditor::addDelaySyncControl (
    DelaySyncControl& control,
    const juce::String& syncParamID,
    const juce::String& divisionParamID)
{
    control.syncButton.setButtonText ("Sync");
    control.syncButton.setColour (juce::ToggleButton::textColourId,
                                  juce::Colours::white.withAlpha (0.8f));
    control.syncButton.setColour (juce::ToggleButton::tickColourId, kAccent);
    addAndMakeVisible (control.syncButton);

    int itemId = 1;
    for (const auto& division : DelaySync::divisions)
        control.divisionBox.addItem (division.label, itemId++);
    control.divisionBox.setJustificationType (juce::Justification::centred);
    control.divisionBox.setColour (juce::ComboBox::backgroundColourId, kBoxFill);
    control.divisionBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    control.divisionBox.setColour (juce::ComboBox::outlineColourId, kAccent.withAlpha (0.35f));
    addAndMakeVisible (control.divisionBox);

    control.buttonAttachment = std::make_unique<ButtonAttachment> (
        proc.apvts, syncParamID, control.syncButton);
    control.divisionAttachment = std::make_unique<ComboBoxAttachment> (
        proc.apvts, divisionParamID, control.divisionBox);
}

void ParticleDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);
}

void ParticleDelayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    titleLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (6);

    // Particle box on top.
    particleView.setBounds (area.removeFromTop (170));
    area.removeFromTop (10);

    // Three rows of four knobs.
    const int rows = 3;
    const int cols = 4;
    const int rowH = area.getHeight() / rows;

    for (int row = 0; row < rows; ++row)
    {
        auto rowArea = area.removeFromTop (rowH);
        const int colW = rowArea.getWidth() / cols;

        for (int col = 0; col < cols; ++col)
        {
            const int index = row * cols + col;
            auto cell = rowArea.removeFromLeft (colW).reduced (4);

            knobs[(size_t) index].label.setBounds (cell.removeFromTop (16));

            if (index == 8 || index == 9)
            {
                auto syncArea = cell.removeFromBottom (24);
                auto& syncControl = delaySyncControls[(size_t) (index - 8)];
                syncControl.syncButton.setBounds (syncArea.removeFromLeft (54));
                syncArea.removeFromLeft (3);
                syncControl.divisionBox.setBounds (syncArea);
            }

            knobs[(size_t) index].slider.setBounds (cell);
        }
    }
}
