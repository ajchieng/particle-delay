#pragma once

#include <JuceHeader.h>

//==============================================================================
// Shared colour palette for the Figma-matched dark instrument editor.
namespace ParticlePalette
{
    inline const juce::Colour shellTop    { 0xff0f1219 };
    inline const juce::Colour shellBottom { 0xff090b11 };
    inline const juce::Colour field       { 0xff06070d };
    inline const juce::Colour panelFill   { 0xff0d1018 };
    inline const juce::Colour accentCyan  { 0xff00ccff };
    inline const juce::Colour accentWarm  { 0xffff8c42 };
    inline const juce::Colour accentBlue  { 0xff4db8ff };
    inline const juce::Colour accentIce   { 0xff7dd4fc };
    inline const juce::Colour accentSpace { 0xffa5b4fc };
    inline const juce::Colour accentMod   { 0xffc4b5fd };
    inline const juce::Colour textMuted   { 0xff243040 };
    inline const juce::Colour textFaint   { 0xff1e2d3d };
    // Use direct ARGB values here. Initialising namespace-scope colours from
    // juce::Colours can run before JUCE's colour globals are constructed.
    inline const juce::Colour textPrimary { 0xffe0eeff };
    inline const juce::Colour textDim     { 0xff5d7084 };

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

    static juce::Font monoFont (float height, int styleFlags = juce::Font::plain);
    static juce::Font displayFont (float height, int styleFlags = juce::Font::plain);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    juce::Label* createComboBoxTextBox (juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleLookAndFeel)
};
