#include "PluginEditor.h"

namespace
{
    // Knob order, grouped by section so layout is index-contiguous:
    //   [0..4]  PHYSICS   [5..7] CAPTURE   [8..9] DELAY   [10..11] OUTPUT
    struct KnobDef { const char* id; const char* name; };
    const std::array<KnobDef, 12> knobDefs {{
        { "PARTICLES",     "Particles"      }, // 0  PHYSICS
        { "GRAVITY",       "Gravity"        }, // 1
        { "BOUNCE",        "Bounce"         }, // 2
        { "SCATTER",       "Scatter"        }, // 3
        { "DECAY",         "Decay"          }, // 4
        { "THRESHOLD",     "Threshold"      }, // 5  CAPTURE
        { "CAPTURE_MAX_MS","Capture Length" }, // 6
        { "SMOOTHNESS",    "Smoothness"     }, // 7
        { "DELAY_MIN_MS",  "Delay Min"      }, // 8  DELAY
        { "DELAY_MAX_MS",  "Delay Max"      }, // 9
        { "MIX",           "Mix"            }, // 10 OUTPUT
        { "OUTPUT",        "Output"         }, // 11
    }};

    // Delay knobs carry an extra tempo-sync row; tracked by name, not a literal.
    constexpr int kDelayMinKnob = 8;
    constexpr int kDelayMaxKnob = 9;
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
    updateTrail();
    repaint();
}

void ParticleView::paintBoxBackground (juce::Graphics& g, juce::Rectangle<float> area, float alpha)
{
    juce::ColourGradient grad (ParticlePalette::panelFill.brighter (0.05f).withAlpha (alpha),
                               area.getCentreX(), area.getY(),
                               ParticlePalette::panelFill.darker (0.28f).withAlpha (alpha),
                               area.getCentreX(), area.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (area);
}

void ParticleView::updateTrail()
{
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0)
        return;

    // (Re)allocate the buffer to match the component and prime it once.
    if (! trailBuffer.isValid() || trailBuffer.getWidth() != w || trailBuffer.getHeight() != h)
    {
        trailBuffer = juce::Image (juce::Image::RGB, w, h, false);
        juce::Graphics ig (trailBuffer);
        paintBoxBackground (ig, trailBuffer.getBounds().toFloat(), 1.0f);
    }

    juce::Graphics ig (trailBuffer);
    const auto area = trailBuffer.getBounds().toFloat();

    // Fade the previous frame toward the background: leftover dots become trails.
    paintBoxBackground (ig, area, 0.20f);

    // Pan reference (faint vertical centre line).
    ig.setColour (juce::Colours::white.withAlpha (0.05f));
    ig.drawVerticalLine ((int) area.getCentreX(), area.getY(), area.getBottom());

    // Warm glow band along the floor, where bounces fire echoes.
    const float glowH = area.getHeight() * 0.30f;
    juce::ColourGradient floorGlow (ParticlePalette::accentWarm.withAlpha (0.0f),
                                    area.getCentreX(), area.getBottom() - glowH,
                                    ParticlePalette::accentWarm.withAlpha (0.16f),
                                    area.getCentreX(), area.getBottom(), false);
    ig.setGradientFill (floorGlow);
    ig.fillRect (area.withTop (area.getBottom() - glowH));

    // Floor line.
    ig.setColour (ParticlePalette::accentCyan.withAlpha (0.5f));
    ig.drawHorizontalLine ((int) area.getBottom() - 1, area.getX(), area.getRight());

    // Particles: colour blends cyan -> amber with energy; size/opacity follow it.
    for (int i = 0; i < numDots; ++i)
    {
        const auto& d = dots[(size_t) i];
        const float energy = juce::jlimit (0.0f, 1.0f, d.energy);

        const float cx = area.getX() + d.x * area.getWidth();
        const float cy = area.getY() + d.y * area.getHeight();
        const float r  = 2.0f + energy * 6.0f;
        const auto  col = ParticlePalette::energyColour (energy);

        // Extra warm flare for energetic particles near the floor (impact).
        if (d.y > 0.86f)
        {
            const float impact = energy * juce::jlimit (0.0f, 1.0f, (d.y - 0.86f) / 0.14f);
            const float fr = r * 3.5f;
            ig.setColour (ParticlePalette::accentWarm.withAlpha (0.28f * impact));
            ig.fillEllipse (cx - fr, area.getBottom() - fr, fr * 2.0f, fr * 2.0f);
        }

        ig.setColour (col.withAlpha (0.15f + energy * 0.4f));
        ig.fillEllipse (cx - r * 1.8f, cy - r * 1.8f, r * 3.6f, r * 3.6f); // glow
        ig.setColour (juce::Colours::white.withAlpha (0.4f + energy * 0.5f));
        ig.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);              // core
    }
}

void ParticleView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    juce::Path clip;
    clip.addRoundedRectangle (bounds, 6.0f);

    if (trailBuffer.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (clip);
        g.drawImageAt (trailBuffer, 0, 0);
    }
    else
    {
        g.setColour (ParticlePalette::panelFill);
        g.fillRoundedRectangle (bounds, 6.0f);
    }

    g.setColour (ParticlePalette::accentCyan.withAlpha (0.35f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.5f);

    if (numDots == 0)
    {
        g.setColour (ParticlePalette::textDim);
        g.setFont (14.0f);
        g.drawText ("feed me a transient", bounds, juce::Justification::centred);
    }
}

//==============================================================================
ParticleDelayAudioProcessorEditor::ParticleDelayAudioProcessorEditor (ParticleDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), particleView (p)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("PARTICLE DELAY", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, ParticlePalette::accentCyan);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    addAndMakeVisible (particleView);

    for (int i = 0; i < numKnobs; ++i)
        addKnob (knobs[(size_t) i], knobDefs[(size_t) i].id, knobDefs[(size_t) i].name);

    addDelaySyncControl (delaySyncControls[0], "DELAY_MIN_SYNC", "DELAY_MIN_DIV");
    addDelaySyncControl (delaySyncControls[1], "DELAY_MAX_SYNC", "DELAY_MAX_DIV");

    sections[0].title = "PHYSICS";
    sections[1].title = "CAPTURE";
    sections[2].title = "DELAY";
    sections[3].title = "OUTPUT";

    setSize (820, 660);
}

ParticleDelayAudioProcessorEditor::~ParticleDelayAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ParticleDelayAudioProcessorEditor::addKnob (Knob& knob,
                                                 const juce::String& paramID,
                                                 const juce::String& displayName)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setColour (juce::Slider::textBoxTextColourId,    ParticlePalette::textPrimary);
    knob.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (knob.slider);
    // Create the internal value Label only after the slider inherits this
    // editor's LookAndFeel. This avoids host/default colours being baked in.
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);

    knob.label.setText (displayName, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setColour (juce::Label::textColourId, ParticlePalette::textPrimary);
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
    addAndMakeVisible (control.syncButton);

    int itemId = 1;
    for (const auto& division : DelaySync::divisions)
        control.divisionBox.addItem (division.label, itemId++);
    control.divisionBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (control.divisionBox);

    control.buttonAttachment = std::make_unique<ButtonAttachment> (
        proc.apvts, syncParamID, control.syncButton);
    control.divisionAttachment = std::make_unique<ComboBoxAttachment> (
        proc.apvts, divisionParamID, control.divisionBox);
}

void ParticleDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace ParticlePalette;

    // Vertical background gradient for depth.
    auto full = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (bgTop,    full.getCentreX(), full.getY(),
                                             bgBottom, full.getCentreX(), full.getBottom(), false));
    g.fillRect (full);

    // Thin accent rule under the title.
    const float ruleY = (float) titleLabel.getBottom() + 3.0f;
    g.setColour (accentCyan.withAlpha (0.5f));
    g.drawLine ((float) titleLabel.getX(), ruleY, full.getRight() - 16.0f, ruleY, 1.0f);

    // Section cards: drop shadow, fill, hairline border, header.
    for (const auto& s : sections)
    {
        if (s.bounds.isEmpty())
            continue;

        juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 12, { 0, 4 })
            .drawForRectangle (g, s.bounds);

        auto pf = s.bounds.toFloat();
        g.setColour (panelFill);
        g.fillRoundedRectangle (pf, 8.0f);
        g.setColour (accentCyan.withAlpha (0.12f));
        g.drawRoundedRectangle (pf, 8.0f, 1.0f);

        g.setColour (accentCyan.withAlpha (0.65f));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (s.title, s.bounds.reduced (12, 0).withHeight (20).withY (s.bounds.getY() + 6),
                    juce::Justification::centredLeft);
    }
}

void ParticleDelayAudioProcessorEditor::layoutKnobRow (juce::Rectangle<int> inner, int startIndex, int count)
{
    if (count <= 0)
        return;

    const int cellW = inner.getWidth() / count;

    for (int i = 0; i < count; ++i)
    {
        const int index = startIndex + i;
        // Give the last cell any rounding remainder.
        auto cell = inner.removeFromLeft (i == count - 1 ? inner.getWidth() : cellW).reduced (6);

        knobs[(size_t) index].label.setBounds (cell.removeFromTop (16));

        if (index == kDelayMinKnob || index == kDelayMaxKnob)
        {
            auto syncArea = cell.removeFromBottom (24);
            auto& syncControl = delaySyncControls[(size_t) (index - kDelayMinKnob)];
            syncControl.syncButton.setBounds (syncArea.removeFromLeft (juce::jmin (50, syncArea.getWidth() / 2)));
            syncArea.removeFromLeft (4);
            syncControl.divisionBox.setBounds (syncArea);
            cell.removeFromBottom (4);
        }

        knobs[(size_t) index].slider.setBounds (cell);
    }
}

void ParticleDelayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    titleLabel.setBounds (area.removeFromTop (30));
    area.removeFromTop (10); // headroom for the accent rule drawn in paint()

    particleView.setBounds (area.removeFromTop (180));
    area.removeFromTop (14);

    const int gap = 10;

    // Row A: PHYSICS spans the full width.
    sections[0].bounds = area.removeFromTop (175);
    area.removeFromTop (gap);

    // Row B: CAPTURE | DELAY | OUTPUT, widths weighted by knob count (3:2:2).
    auto rowB = area;
    const int totalW   = rowB.getWidth() - 2 * gap;
    const int wCapture = totalW * 3 / 7;
    const int wDelay   = totalW * 2 / 7;

    sections[1].bounds = rowB.removeFromLeft (wCapture);
    rowB.removeFromLeft (gap);
    sections[2].bounds = rowB.removeFromLeft (wDelay);
    rowB.removeFromLeft (gap);
    sections[3].bounds = rowB; // OUTPUT takes the remainder

    // Inner content area of a card: padding, minus the header strip.
    auto innerOf = [] (juce::Rectangle<int> b)
    {
        b = b.reduced (8);
        b.removeFromTop (18);
        return b;
    };

    layoutKnobRow (innerOf (sections[0].bounds), 0,  5); // PHYSICS
    layoutKnobRow (innerOf (sections[1].bounds), 5,  3); // CAPTURE
    layoutKnobRow (innerOf (sections[2].bounds), 8,  2); // DELAY
    layoutKnobRow (innerOf (sections[3].bounds), 10, 2); // OUTPUT
}
