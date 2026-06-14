#include "ParticleLookAndFeel.h"
#include <cmath>

using namespace ParticlePalette;

//==============================================================================
ParticleLookAndFeel::ParticleLookAndFeel()
{
    // Make light text the default for every label created under this dark
    // LookAndFeel, including labels JUCE creates internally for controls.
    setColour (juce::Label::textColourId,                 textPrimary);
    setColour (juce::TextEditor::textColourId,            textPrimary);
    setColour (juce::TextEditor::backgroundColourId,      panelFill.darker (0.20f));
    setColour (juce::TextEditor::outlineColourId,         accentCyan.withAlpha (0.35f));
    setColour (juce::TextEditor::focusedOutlineColourId,  accentCyan);
    setColour (juce::TextEditor::highlightColourId,       accentCyan.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId,          textPrimary);
    setColour (juce::TooltipWindow::textColourId,         textPrimary);
    setColour (juce::TooltipWindow::backgroundColourId,   panelFill.darker (0.25f));
    setColour (juce::TooltipWindow::outlineColourId,      accentCyan.withAlpha (0.35f));

    // Dark-themed popup menu for the tempo-division dropdowns.
    setColour (juce::PopupMenu::backgroundColourId,            panelFill.darker (0.25f));
    setColour (juce::PopupMenu::textColourId,                  textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentCyan.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);

    // Slider value read-outs: readable text, no box chrome (the knob carries the
    // look). The editor also sets this per-slider before the text box is built,
    // since a freshly-created slider resolves colours against the default LAF.
    setColour (juce::Slider::textBoxTextColourId,       textPrimary);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    setColour (juce::ComboBox::backgroundColourId, panelFill);
    setColour (juce::ComboBox::outlineColourId,    accentCyan.withAlpha (0.35f));
    setColour (juce::ComboBox::arrowColourId,      accentCyan);
}

//==============================================================================
void ParticleLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto centre = bounds.getCentre();
    const float radius   = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float lineW    = juce::jmax (2.5f, radius * 0.12f);
    const float arcR     = radius - lineW * 0.5f - 2.0f;
    const float angle    = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float bodyR    = juce::jmax (4.0f, arcR - lineW * 0.5f - 3.0f);

    const bool active = slider.isMouseButtonDown() || slider.isMouseOverOrDragging();
    const juce::Colour arcColour = active ? accentWarm : accentCyan;

    // Knob body: a slightly lit disc for depth.
    {
        juce::ColourGradient grad (panelFill.brighter (0.22f),
                                   centre.x - bodyR * 0.4f, centre.y - bodyR * 0.5f,
                                   panelFill.darker (0.40f),
                                   centre.x, centre.y + bodyR, true);
        g.setGradientFill (grad);
        g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);
    }

    // Background track arc.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc with a soft glow underneath.
    if (sliderPos > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);

        g.setColour (arcColour.withAlpha (0.22f));
        g.strokePath (value, juce::PathStrokeType (lineW * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (arcColour);
        g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer. addCentredArc measures angle from 12 o'clock, clockwise positive:
    //   x = cx + sin(theta) * r,  y = cy - cos(theta) * r
    const juce::Point<float> tip  { centre.x + std::sin (angle) * (bodyR - 2.0f),
                                    centre.y - std::cos (angle) * (bodyR - 2.0f) };
    const juce::Point<float> root { centre.x + std::sin (angle) * (bodyR * 0.30f),
                                    centre.y - std::cos (angle) * (bodyR * 0.30f) };
    g.setColour (active ? accentWarm : juce::Colours::white.withAlpha (0.9f));
    g.drawLine ({ root, tip }, juce::jmax (2.0f, lineW * 0.6f));
}

juce::Label* ParticleLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);

    label->setColour (juce::Label::textColourId, ParticlePalette::textPrimary);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::TextEditor::textColourId, ParticlePalette::textPrimary);
    label->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);

    return label;
}

//==============================================================================
void ParticleLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                        int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                        juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const float corner = 4.0f;

    g.setColour (panelFill.darker (0.20f));
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (accentCyan.withAlpha (box.isPopupActive() ? 0.55f : 0.25f));
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    // Chevron.
    const float cx = (float) width - 10.0f;
    const float cy = (float) height * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath (cx - 3.0f, cy - 1.6f);
    arrow.lineTo (cx,          cy + 2.0f);
    arrow.lineTo (cx + 3.0f,   cy - 1.6f);
    g.setColour (accentCyan.withAlpha (0.85f));
    g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Label* ParticleLookAndFeel::createComboBoxTextBox (juce::ComboBox& box)
{
    auto* label = juce::LookAndFeel_V4::createComboBoxTextBox (box);

    label->setColour (juce::Label::textColourId, ParticlePalette::textPrimary);
    label->setColour (juce::TextEditor::textColourId, ParticlePalette::textPrimary);

    return label;
}

juce::Font ParticleLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (13.0f));
}

void ParticleLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 20, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, box.findColour (juce::ComboBox::textColourId));
    label.setColour (juce::TextEditor::textColourId, box.findColour (juce::ComboBox::textColourId));
}

//==============================================================================
void ParticleLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                            bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();
    const float corner = bounds.getHeight() * 0.5f;

    g.setColour (on ? accentWarm.withAlpha (0.22f) : juce::Colours::white.withAlpha (0.04f));
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (on ? accentWarm.withAlpha (0.9f)
                    : juce::Colours::white.withAlpha (shouldDrawButtonAsHighlighted ? 0.45f : 0.22f));
    g.drawRoundedRectangle (bounds, corner, 1.2f);

    g.setColour (on ? accentWarm : juce::Colours::white.withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
}
