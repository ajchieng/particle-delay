#include "ParticleLookAndFeel.h"
#include "BinaryData.h"
#include <cmath>

using namespace ParticlePalette;

namespace
{
    juce::Typeface::Ptr typefaceFromData (const void* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    juce::Colour sliderAccent (juce::Slider& slider)
    {
        return slider.findColour (juce::Slider::rotarySliderFillColourId);
    }
}

//==============================================================================
ParticleLookAndFeel::ParticleLookAndFeel()
{
    // Make light text the default for every label created under this dark
    // LookAndFeel, including labels JUCE creates internally for controls.
    setColour (juce::Label::textColourId,                 textPrimary);
    setColour (juce::TextEditor::textColourId,            textPrimary);
    setColour (juce::TextEditor::backgroundColourId,      field);
    setColour (juce::TextEditor::outlineColourId,         accentCyan.withAlpha (0.35f));
    setColour (juce::TextEditor::focusedOutlineColourId,  accentCyan);
    setColour (juce::TextEditor::highlightColourId,       accentCyan.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId,          textPrimary);
    setColour (juce::TooltipWindow::textColourId,         textPrimary);
    setColour (juce::TooltipWindow::backgroundColourId,   panelFill);
    setColour (juce::TooltipWindow::outlineColourId,      accentCyan.withAlpha (0.35f));

    // Dark-themed popup menu for the tempo-division dropdowns.
    setColour (juce::PopupMenu::backgroundColourId,            panelFill);
    setColour (juce::PopupMenu::textColourId,                  textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentCyan.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);

    // Slider value read-outs: readable text, no box chrome (the knob carries the
    // look). The editor also sets this per-slider before the text box is built,
    // since a freshly-created slider resolves colours against the default LAF.
    setColour (juce::Slider::textBoxTextColourId,       textPrimary);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::ComboBox::textColourId,       accentCyan);
    setColour (juce::ComboBox::backgroundColourId, accentCyan.withAlpha (0.05f));
    setColour (juce::ComboBox::outlineColourId,    accentCyan.withAlpha (0.20f));
    setColour (juce::ComboBox::arrowColourId,      accentCyan);
}

juce::Font ParticleLookAndFeel::monoFont (float height, int styleFlags)
{
    static auto typeface = typefaceFromData (BinaryData::GeistMonoRegular_ttf,
                                             BinaryData::GeistMonoRegular_ttfSize);
    auto options = typeface != nullptr ? juce::FontOptions (typeface).withHeight (height)
                                       : juce::FontOptions (height);
    return juce::Font (options).withStyle (styleFlags);
}

juce::Font ParticleLookAndFeel::displayFont (float height, int styleFlags)
{
    static auto regular = typefaceFromData (BinaryData::BarlowCondensedRegular_ttf,
                                            BinaryData::BarlowCondensedRegular_ttfSize);
    static auto semiBold = typefaceFromData (BinaryData::BarlowCondensedSemiBold_ttf,
                                             BinaryData::BarlowCondensedSemiBold_ttfSize);
    auto chosen = (styleFlags & juce::Font::bold) != 0 ? semiBold : regular;
    auto options = chosen != nullptr ? juce::FontOptions (chosen).withHeight (height)
                                     : juce::FontOptions (height);
    return juce::Font (options).withStyle (styleFlags);
}

//==============================================================================
void ParticleLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    // The value read-out lives in the centre of the knob (see getSliderLayout),
    // so the dial is free to fill the whole cell instead of reserving a row.
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const auto centre = bounds.getCentre();
    const float radius   = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float lineW    = juce::jlimit (2.5f, 6.0f, radius * 0.07f);
    const float arcR     = radius - 4.0f;
    const float angle    = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float bodyR    = juce::jmax (4.0f, arcR - 12.0f);

    const bool active = slider.isMouseButtonDown() || slider.isMouseOverOrDragging();
    const juce::Colour arcColour = active ? sliderAccent (slider).brighter (0.25f)
                                          : sliderAccent (slider);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour { 0x12ffffff });
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (sliderPos > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);

        g.setColour (arcColour.withAlpha (0.30f));
        g.strokePath (value, juce::PathStrokeType (lineW * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (arcColour);
        g.strokePath (value, juce::PathStrokeType (lineW * 1.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (field);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colour { 0x0affffff });
    g.drawEllipse (centre.x - bodyR + 1.0f, centre.y - bodyR + 1.0f,
                   bodyR * 2.0f - 2.0f, bodyR * 2.0f - 2.0f, 1.0f);

    for (float t : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        const float tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const auto inner = centre + juce::Point<float> { std::sin (tickAngle) * (arcR + 2.0f),
                                                        -std::cos (tickAngle) * (arcR + 2.0f) };
        const auto outer = centre + juce::Point<float> { std::sin (tickAngle) * (arcR + 4.0f),
                                                        -std::cos (tickAngle) * (arcR + 4.0f) };
        g.setColour (juce::Colour { 0x1fffffff });
        g.drawLine ({ inner, outer }, 1.0f);
    }

    const auto dot = centre + juce::Point<float> { std::sin (angle) * (arcR - 4.0f),
                                                  -std::cos (angle) * (arcR - 4.0f) };
    g.setColour (arcColour.withAlpha (0.35f));
    g.fillEllipse (dot.x - 6.5f, dot.y - 6.5f, 13.0f, 13.0f);
    g.setColour (arcColour);
    g.fillEllipse (dot.x - 3.0f, dot.y - 3.0f, 6.0f, 6.0f);
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
    label->setFont (monoFont (14.5f));
    label->setJustificationType (juce::Justification::centred);

    // The value read-out sits in a band across the centre of the knob. Let mouse
    // events fall through the label to the slider beneath so the whole dial is a
    // drag surface (otherwise the label would swallow clicks over the middle of the
    // knob). The second flag keeps child clicks live so the text editor that opens
    // on double-click still receives clicks for caret placement and selection.
    label->setInterceptsMouseClicks (false, true);

    return label;
}

juce::Slider::SliderLayout ParticleLookAndFeel::getSliderLayout (juce::Slider& slider)
{
    // Knob fills the whole component; the value sits in a band across its centre.
    juce::Slider::SliderLayout layout;
    auto b = slider.getLocalBounds();

    const int textH = 24;
    layout.textBoxBounds = juce::Rectangle<int> (b.getX(), b.getCentreY() - textH / 2,
                                                 b.getWidth(), textH);
    layout.sliderBounds = b;
    return layout;
}

//==============================================================================
void ParticleLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                        int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                        juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const float corner = 3.0f;

    g.setColour (accentCyan.withAlpha (0.05f));
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (accentCyan.withAlpha (box.isPopupActive() ? 0.45f : 0.20f));
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    // Chevron.
    const float cx = (float) width - 8.0f;
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

    label->setColour (juce::Label::textColourId, accentCyan);
    label->setColour (juce::TextEditor::textColourId, accentCyan);
    label->setFont (monoFont (12.5f));

    return label;
}

juce::Font ParticleLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return monoFont (12.5f);
}

void ParticleLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (7, 1, box.getWidth() - 18, box.getHeight() - 2);
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
    const float corner = 2.0f;
    const auto colour = button.findColour (juce::ToggleButton::tickColourId);

    g.setColour (on ? colour.withAlpha (0.14f) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (on ? colour.withAlpha (0.66f)
                    : juce::Colours::white.withAlpha (shouldDrawButtonAsHighlighted ? 0.14f : 0.07f));
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    g.setColour (on ? colour : textDim);
    g.setFont (monoFont (13.5f));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
}

void ParticleLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (backgroundColour);

    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto accent = button.findColour (juce::TextButton::textColourOffId);
    const bool enabled = button.isEnabled();
    const bool toggled = button.getToggleState();
    const auto border = enabled ? accent.withAlpha (toggled ? 0.50f
                                              : shouldDrawButtonAsHighlighted ? 0.35f : 0.16f)
                                : juce::Colours::white.withAlpha (0.07f);
    const auto fill = toggled ? accent.withAlpha (0.14f)
                    : shouldDrawButtonAsDown ? accent.withAlpha (0.12f)
                    : shouldDrawButtonAsHighlighted ? accent.withAlpha (0.08f)
                    : juce::Colours::transparentBlack;

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (border);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
}

void ParticleLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    g.setFont (monoFont (12.5f));
    g.setColour (button.isEnabled() ? button.findColour (juce::TextButton::textColourOffId)
                                         .withAlpha (button.getToggleState() ? 1.0f : 0.75f)
                                    : textDim);
    g.drawText (button.getButtonText(), button.getLocalBounds().reduced (4, 1),
                juce::Justification::centred);
}
