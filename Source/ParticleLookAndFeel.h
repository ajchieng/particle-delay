#pragma once

#include <JuceHeader.h>

//==============================================================================
// Shared colour palette for the editor. A dark navy base with a cyan primary
// accent and a warm amber secondary: idle controls read cyan, while active /
// high-energy elements blend toward amber so they pop.
namespace ParticlePalette
{
    inline const juce::Colour bgTop       { 0xff131722 };
    inline const juce::Colour bgBottom    { 0xff0a0c12 };
    inline const juce::Colour panelFill   { 0xff161c28 };
    inline const juce::Colour accentCyan  { 0xff4fd0e0 };
    inline const juce::Colour accentWarm  { 0xffff7a59 };
    // Use direct ARGB values here. Initialising namespace-scope colours from
    // juce::Colours can run before JUCE's colour globals are constructed.
    inline const juce::Colour textPrimary { 0xd9ffffff };
    inline const juce::Colour textDim     { 0x73ffffff };

    // Blend cyan -> amber by t (0 = idle/cold, 1 = hot/active).
    inline juce::Colour energyColour (float t)
    {
        return accentCyan.interpolatedWith (accentWarm, juce::jlimit (0.0f, 1.0f, t));
    }
}

//==============================================================================
// Custom look for the whole editor: drawn rotary knobs with a glowing value arc
// and warm drag feedback, dark rounded combo boxes / popups, and pill-style
// toggle buttons. All procedural - no image assets.
class ParticleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ParticleLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    juce::Label* createComboBoxTextBox (juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleLookAndFeel)
};
