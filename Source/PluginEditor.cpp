#include "PluginEditor.h"

namespace
{
    // Knob order, grouped by the original editor sections:
    //   [0..4] PHYSICS  [5..7] CAPTURE  [8..9] DELAY
    //   [10..11] OUTPUT  [12..17] SPACE
    struct KnobDef { const char* id; const char* name; const char* tip; };
    const std::array<KnobDef, 18> knobDefs {{
        { "PARTICLES",     "Particles",      "Particles launched per detected hit (1-32)." },
        { "GRAVITY",       "Gravity",        "Fall speed and first-impact time; free-running, not tempo-synced." },
        { "BOUNCE",        "Bounce",         "Energy and speed a particle keeps after each floor hit." },
        { "SCATTER",       "Scatter",        "Stereo spread and release-time variation of the burst." },
        { "DECAY",         "Decay",          "How quickly particle energy (and echo level) fades." },
        { "THRESHOLD",     "Threshold",      "Input level needed to trigger a hit. Watch the meter below." },
        { "CAPTURE_MAX_MS","Capture Length", "Maximum stereo audio captured per hit (80-500 ms)." },
        { "SMOOTHNESS",    "Smoothness",     "Attack and release fades applied to each replayed hit." },
        { "DELAY_MIN_MS",  "Delay Min",      "Start of the audible bounce window. Sync to tempo with the button." },
        { "DELAY_MAX_MS",  "Delay Max",      "End of the audible bounce window. Sync to tempo with the button." },
        { "MIX",           "Mix",            "Balance between the dry input and the echoes." },
        { "OUTPUT",        "Output",         "Final output level." },
        { "FEEDBACK",      "Feedback",       "Extends echo-train life past the Bounce/Decay limits." },
        { "WET_HP",        "High Pass",      "High-pass filter on the echoes only." },
        { "WET_LP",        "Low Pass",       "Low-pass filter on the echoes for a darker tail." },
        { "DIFFUSE",       "Diffuse",        "Smears sharp echoes into a softer, reverberant wash." },
        { "DIFFUSE_SIZE",  "Size",           "Length and spread of the diffusion (active when Diffuse is up)." },
        { "WET_WIDTH",     "Width",          "Stereo width of the echoes (0% mono, 200% wide)." },
    }};

    // Delay knobs carry an extra tempo-sync row; tracked by name, not a literal.
    constexpr int kThresholdKnob = 5;
    constexpr int kDelayMinKnob = 8;
    constexpr int kDelayMaxKnob = 9;

    constexpr int editorW = 900;
    constexpr int editorH = 900;
    constexpr int headerH = 48;
    constexpr int footerH = 27;

    juce::Colour sectionAccentForIndex (int index)
    {
        using namespace ParticlePalette;
        if (index <= 4)  return accentIce;
        if (index <= 7)  return accentBlue;
        if (index <= 9)  return accentCyan;
        if (index <= 11) return accentWarm;
        return accentSpace;
    }

    void drawMonoText (juce::Graphics& g, const juce::String& text,
                       juce::Rectangle<int> bounds, float size,
                       juce::Colour colour, juce::Justification justification)
    {
        g.setFont (ParticleLookAndFeel::monoFont (size));
        g.setColour (colour);
        g.drawText (text, bounds, justification, false);
    }
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
    juce::ignoreUnused (alpha);
    g.setColour (ParticlePalette::field.withAlpha (0.18f));
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

    ig.setColour (ParticlePalette::accentCyan.withAlpha (0.03f));
    for (int i = 0; i <= 10; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / 10.0f;
        ig.drawVerticalLine ((int) std::round (x), area.getY(), area.getBottom());
    }
    for (int i = 0; i <= 5; ++i)
    {
        const float y = area.getY() + area.getHeight() * (float) i / 5.0f;
        ig.drawHorizontalLine ((int) std::round (y), area.getX(), area.getRight());
    }

    juce::Path dashPath;
    dashPath.startNewSubPath (area.getX(), area.getCentreY());
    dashPath.lineTo (area.getRight(), area.getCentreY());
    float dashes[] { 3.0f, 9.0f };
    juce::Path dashedAxis;
    juce::PathStrokeType (1.0f).createDashedStroke (dashedAxis, dashPath, dashes, 2);
    ig.setColour (ParticlePalette::accentCyan.withAlpha (0.07f));
    ig.fillPath (dashedAxis);

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
            ig.setColour (ParticlePalette::accentWarm.withAlpha (0.18f * impact));
            ig.fillEllipse (cx - fr, area.getBottom() - fr, fr * 2.0f, fr * 2.0f);
        }

        ig.setColour (col.withAlpha (0.10f + energy * 0.35f));
        ig.fillEllipse (cx - r * 2.4f, cy - r * 2.4f, r * 4.8f, r * 4.8f); // glow
        ig.setColour (juce::Colour { 0xffd2f5ff }.withAlpha (0.35f + energy * 0.55f));
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
        g.setColour (ParticlePalette::field);
        g.fillRoundedRectangle (bounds, 2.0f);
    }

    g.setColour (ParticlePalette::accentCyan.withAlpha (0.09f));
    g.drawRoundedRectangle (bounds, 2.0f, 1.0f);

    if (numDots == 0)
    {
        g.setColour (ParticlePalette::textDim);
        g.setFont (ParticleLookAndFeel::monoFont (10.0f));
        g.drawText ("FEED ME A TRANSIENT", bounds, juce::Justification::centred);
    }
}

//==============================================================================
MeterView::MeterView (ParticleDelayAudioProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

MeterView::~MeterView() { stopTimer(); }

void MeterView::timerCallback() { repaint(); }

void MeterView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (ParticlePalette::panelFill.darker (0.4f));
    g.fillRoundedRectangle (r, 2.0f);

    // Map both the input level and the Threshold through the parameter's own
    // (skewed) range so the bar edge meets the marker exactly at trigger point.
    const auto range = proc.apvts.getParameterRange ("THRESHOLD");
    const float rawLevel  = proc.getInputLevel();
    const float rawThresh = proc.apvts.getRawParameterValue ("THRESHOLD")->load();

    const float levelPos  = range.convertTo0to1 (juce::jlimit (range.start, range.end, rawLevel));
    const float threshPos = range.convertTo0to1 (juce::jlimit (range.start, range.end, rawThresh));

    if (levelPos > 0.0f)
    {
        const bool over = rawLevel >= rawThresh;
        g.setColour ((over ? ParticlePalette::accentWarm : ParticlePalette::accentCyan)
                         .withAlpha (0.85f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * levelPos), 2.0f);
    }

    const float mx = r.getX() + r.getWidth() * threshPos;
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.fillRect (mx - 0.75f, r.getY(), 1.5f, r.getHeight());
}

//==============================================================================
ParticleDelayAudioProcessorEditor::ParticleDelayAudioProcessorEditor (ParticleDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), particleView (p), thresholdMeter (p)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("PARTICLE DELAY", juce::dontSendNotification);
    titleLabel.setFont (ParticleLookAndFeel::displayFont (26.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, ParticlePalette::textPrimary);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    resetButton.setButtonText ("INIT");
    resetButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    resetButton.setColour (juce::TextButton::textColourOffId, ParticlePalette::textDim);
    resetButton.setTooltip ("Reset all parameters to their defaults");
    resetButton.onClick = [this] { proc.resetToDefaults(); };
    addAndMakeVisible (resetButton);

    helpButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    helpButton.setColour (juce::TextButton::textColourOffId, ParticlePalette::textDim);
    helpButton.setTooltip ("Open the help panel");
    helpButton.onClick = [this]
    {
        helpPanel.setVisible (true);
        helpPanel.toFront (true);
    };
    addAndMakeVisible (helpButton);

    addAndMakeVisible (particleView);
    addAndMakeVisible (thresholdMeter);

    helpPanel.onClose = [this] { helpPanel.setVisible (false); };
    addChildComponent (helpPanel); // hidden until the "?" button shows it

    // Preset bar.
    auto styleBarButton = [this] (juce::TextButton& b, const juce::String& tip)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b.setColour (juce::TextButton::textColourOffId, ParticlePalette::textDim);
        b.setTooltip (tip);
        addAndMakeVisible (b);
    };
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setTooltip ("Load a factory or user preset");
    presetBox.onChange = [this] { applySelectedPreset(); };
    addAndMakeVisible (presetBox);

    prevPresetButton.onClick = [this] { stepPreset (-1); };
    nextPresetButton.onClick = [this] { stepPreset (1); };
    savePresetButton.setButtonText ("SAVE");
    savePresetButton.onClick = [this] { promptSaveUserPreset(); };
    styleBarButton (prevPresetButton, "Previous preset");
    styleBarButton (nextPresetButton, "Next preset");
    styleBarButton (savePresetButton, "Save the current settings as a user preset");
    prevPresetButton.setVisible (false);
    nextPresetButton.setVisible (false);

    compareAButton.onClick = [this] { switchCompareSlot (0); };
    compareBButton.onClick = [this] { switchCompareSlot (1); };
    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass the effect and pass the input through");
    styleBarButton (compareAButton, "Recall/compare slot A");
    styleBarButton (compareBButton, "Recall/compare slot B");
    styleBarButton (bypassButton, "Bypass the effect and pass the input through");
    compareAButton.setColour (juce::TextButton::textColourOffId, ParticlePalette::accentCyan);
    compareBButton.setColour (juce::TextButton::textColourOffId, ParticlePalette::accentCyan);
    bypassButton.setColour (juce::TextButton::textColourOffId, ParticlePalette::accentWarm);
    bypassAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "BYPASS", bypassButton);

    refreshPresetList();

    for (int i = 0; i < numKnobs; ++i)
        addKnob (knobs[(size_t) i], knobDefs[(size_t) i].id,
                 knobDefs[(size_t) i].name, knobDefs[(size_t) i].tip);

    addDelaySyncControl (delaySyncControls[0], "DELAY_MIN_SYNC", "DELAY_MIN_DIV");
    addDelaySyncControl (delaySyncControls[1], "DELAY_MAX_SYNC", "DELAY_MAX_DIV");

    sections[0].title = "PHYSICS";
    sections[0].accent = ParticlePalette::accentIce;
    sections[1].title = "CAPTURE";
    sections[1].accent = ParticlePalette::accentBlue;
    sections[2].title = "DELAY";
    sections[2].accent = ParticlePalette::accentCyan;
    sections[3].title = "OUTPUT";
    sections[3].accent = ParticlePalette::accentWarm;
    sections[4].title = "SPACE";
    sections[4].accent = ParticlePalette::accentSpace;

    compareStates[0] = proc.apvts.copyState();
    compareStates[1] = proc.apvts.copyState();
    refreshHeaderButtonState();

    setSize (editorW, editorH);
}

ParticleDelayAudioProcessorEditor::~ParticleDelayAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ParticleDelayAudioProcessorEditor::addKnob (Knob& knob,
                                                 const juce::String& paramID,
                                                 const juce::String& displayName,
                                                 const juce::String& tooltipText)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setRotaryParameters (juce::degreesToRadians (-135.0f),
                                     juce::degreesToRadians (135.0f), true);
    knob.slider.setColour (juce::Slider::rotarySliderFillColourId,
                           sectionAccentForIndex ((int) (&knob - knobs.data())));
    knob.slider.setColour (juce::Slider::textBoxTextColourId,    ParticlePalette::textPrimary);
    knob.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.slider.setTooltip (tooltipText);
    addAndMakeVisible (knob.slider);
    // Create the internal value Label only after the slider inherits this
    // editor's LookAndFeel. This avoids host/default colours being baked in.
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 128, 32);

    knob.label.setText (displayName, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setColour (juce::Label::textColourId, ParticlePalette::textDim);
    knob.label.setFont (ParticleLookAndFeel::monoFont (15.0f));
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramID, knob.slider);
}

void ParticleDelayAudioProcessorEditor::addDelaySyncControl (
    DelaySyncControl& control,
    const juce::String& syncParamID,
    const juce::String& divisionParamID)
{
    control.syncButton.setButtonText ("Sync");
    control.syncButton.setColour (juce::ToggleButton::tickColourId, ParticlePalette::accentCyan);
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

    auto full = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (shellTop, full.getCentreX(), full.getY(),
                                             shellBottom, full.getCentreX(), full.getBottom(), false));
    g.fillRoundedRectangle (full.reduced (0.5f), 6.0f);

    g.setColour (accentCyan.withAlpha (0.10f));
    g.drawRoundedRectangle (full.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.90f));
    g.drawRoundedRectangle (full.reduced (1.5f), 5.0f, 1.0f);

    for (int y = 0; y < getHeight(); y += 3)
    {
        g.setColour (accentCyan.withAlpha (0.02f));
        g.drawHorizontalLine (y + 2, 0.0f, (float) getWidth());
    }

    g.setColour (accentCyan.withAlpha (0.08f));
    g.drawHorizontalLine (headerH, 0.0f, (float) getWidth());
    g.drawHorizontalLine (getHeight() - footerH, 0.0f, (float) getWidth());

    // Logo mark from the Figma mockup.
    const auto logo = juce::Rectangle<float> (17.0f, 11.0f, 26.0f, 26.0f);
    g.setColour (accentCyan.withAlpha (0.18f));
    g.drawEllipse (logo, 1.0f);
    const juce::Point<float> p1 { logo.getCentreX(), logo.getY() + 5.0f };
    const juce::Point<float> p2 { logo.getX() + 5.5f, logo.getBottom() - 6.5f };
    const juce::Point<float> p3 { logo.getRight() - 5.5f, logo.getBottom() - 6.5f };
    g.setColour (accentCyan.withAlpha (0.22f));
    g.drawLine ({ p1, p2 }, 0.8f);
    g.drawLine ({ p1, p3 }, 0.8f);
    g.drawLine ({ p2, p3 }, 0.8f);
    for (auto p : { p1, p2, p3, logo.getCentre() + juce::Point<float> (0.0f, 3.0f) })
    {
        g.setColour (accentCyan.withAlpha (0.20f));
        g.fillEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
        g.setColour (accentCyan.withAlpha (0.78f));
        g.fillEllipse (p.x - 2.0f, p.y - 2.0f, 4.0f, 4.0f);
    }

    drawMonoText (g, "PRESET", { 374, 17, 36, 14 }, 9.0f, textMuted, juce::Justification::centredRight);

    auto fieldBounds = particleView.getBounds().toFloat();
    g.setColour (field);
    g.fillRoundedRectangle (fieldBounds, 2.0f);
    g.setColour (accentCyan.withAlpha (0.09f));
    g.drawRoundedRectangle (fieldBounds, 2.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.60f));
    g.drawRoundedRectangle (fieldBounds.reduced (1.0f), 1.5f, 1.0f);

    drawMonoText (g, "PARTICLE FIELD", particleView.getBounds().reduced (11, 7).withHeight (11),
                  9.5f, accentCyan.withAlpha (0.35f), juce::Justification::centredLeft);

    const int particleCount = (int) proc.apvts.getRawParameterValue ("PARTICLES")->load();
    const float delayMin = proc.apvts.getRawParameterValue ("DELAY_MIN_MS")->load();
    drawMonoText (g, juce::String (particleCount) + " particles  *  " + juce::String ((int) delayMin) + "ms",
                  particleView.getBounds().reduced (11, 7).withHeight (14), 9.5f,
                  accentCyan.withAlpha (0.35f), juce::Justification::centredRight);

    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine (260, 16.0f, (float) editorW - 16.0f);

    for (const auto& s : sections)
    {
        if (s.bounds.isEmpty())
            continue;

        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawVerticalLine (s.bounds.getX(), (float) s.bounds.getY(), (float) s.bounds.getBottom());
        g.setColour (s.accent.withAlpha (0.34f));
        g.fillRect (s.bounds.getX() + 18, s.bounds.getY() + 19, 18, 1);
        drawMonoText (g, s.title, { s.bounds.getX() + 44, s.bounds.getY() + 9, 150, 22 },
                      14.0f, s.accent.withAlpha (0.66f), juce::Justification::centredLeft);
    }

    drawMonoText (g, "CPU --", { 20, editorH - 23, 64, 13 }, 8.5f, textFaint, juce::Justification::centredLeft);
    drawMonoText (g, "LATENCY --", { 90, editorH - 23, 92, 13 }, 8.5f, textFaint, juce::Justification::centredLeft);
    drawMonoText (g, "44100 Hz  *  32-bit float", { 196, editorH - 23, 196, 13 }, 8.5f, textFaint,
                  juce::Justification::centredLeft);
    drawMonoText (g, juce::String (particleCount) + " active particles",
                  { 410, editorH - 23, 142, 13 }, 8.5f, textFaint, juce::Justification::centredLeft);
    drawMonoText (g, "FIELDWORKS AUDIO  *  PARTICLE DELAY 1.0.2  *  VST3 / AU",
                  { 550, editorH - 23, 330, 13 }, 8.5f, textFaint.darker (0.20f),
                  juce::Justification::centredRight);
}

void ParticleDelayAudioProcessorEditor::layoutKnobRow (juce::Rectangle<int> inner, int startIndex, int count)
{
    if (count <= 0)
        return;

    const int cellW = inner.getWidth() / count;

    for (int i = 0; i < count; ++i)
    {
        const int index = startIndex + i;
        auto cell = inner.removeFromLeft (i == count - 1 ? inner.getWidth() : cellW).reduced (10, 0);
        auto labelArea = cell.removeFromTop (26);
        cell.removeFromTop (4);

        if (index == kDelayMinKnob || index == kDelayMaxKnob)
        {
            auto syncArea = cell.removeFromBottom (38);
            auto& syncControl = delaySyncControls[(size_t) (index - kDelayMinKnob)];
            syncControl.syncButton.setBounds (syncArea.removeFromLeft (juce::jmin (64, syncArea.getWidth() / 2)));
            syncArea.removeFromLeft (8);
            syncControl.divisionBox.setBounds (syncArea.reduced (0, 3));
            cell.removeFromBottom (8);
        }
        else if (index == kThresholdKnob)
        {
            thresholdMeter.setBounds (cell.removeFromBottom (12));
            cell.removeFromBottom (8);
        }

        knobs[(size_t) index].slider.setBounds (cell);
        knobs[(size_t) index].label.setBounds (labelArea);
    }
}

void ParticleDelayAudioProcessorEditor::resized()
{
    titleLabel.setBounds (54, 9, 300, 30);

    presetBox.setBounds (413, 13, 105, 23);
    savePresetButton.setBounds (527, 13, 37, 23);
    resetButton.setBounds (572, 13, 37, 23);
    helpButton.setBounds (617, 13, 24, 23);
    compareAButton.setBounds (732, 13, 32, 23);
    compareBButton.setBounds (763, 13, 32, 23);
    bypassButton.setBounds (803, 13, 71, 23);
    prevPresetButton.setBounds ({});
    nextPresetButton.setBounds ({});

    particleView.setBounds (16, 64, editorW - 32, 150);

    const int top = 238;
    const int x0 = 16;
    const int width = editorW - 32;
    const int gap = 12;
    const int rowH = 190;

    sections[0].bounds = { x0, top, width, rowH };

    auto rowB = juce::Rectangle<int> (x0, top + rowH + gap, width, rowH);
    const int totalW = rowB.getWidth() - 2 * gap;
    const int wCapture = totalW * 3 / 7;
    const int wDelay = totalW * 2 / 7;
    sections[1].bounds = rowB.removeFromLeft (wCapture);
    rowB.removeFromLeft (gap);
    sections[2].bounds = rowB.removeFromLeft (wDelay);
    rowB.removeFromLeft (gap);
    sections[3].bounds = rowB;

    sections[4].bounds = { x0, top + (rowH + gap) * 2, width, rowH };

    auto innerOf = [] (juce::Rectangle<int> b)
    {
        b.removeFromTop (42);
        b = b.reduced (8, 0);
        b.removeFromBottom (14);
        return b;
    };

    layoutKnobRow (innerOf (sections[0].bounds), 0,  5); // PHYSICS
    layoutKnobRow (innerOf (sections[1].bounds), 5,  3); // CAPTURE
    layoutKnobRow (innerOf (sections[2].bounds), 8,  2); // DELAY
    layoutKnobRow (innerOf (sections[3].bounds), 10, 2); // OUTPUT
    layoutKnobRow (innerOf (sections[4].bounds), 12, 6); // SPACE

    helpPanel.setBounds (getLocalBounds());
}

void ParticleDelayAudioProcessorEditor::storeActiveCompareState()
{
    compareStates[(size_t) activeCompareSlot] = proc.apvts.copyState();
}

void ParticleDelayAudioProcessorEditor::switchCompareSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeCompareSlot)
        return;

    storeActiveCompareState();
    activeCompareSlot = slot;

    if (compareStates[(size_t) slot].isValid())
        proc.apvts.replaceState (compareStates[(size_t) slot].createCopy());

    refreshHeaderButtonState();
}

void ParticleDelayAudioProcessorEditor::refreshHeaderButtonState()
{
    compareAButton.setToggleState (activeCompareSlot == 0, juce::dontSendNotification);
    compareBButton.setToggleState (activeCompareSlot == 1, juce::dontSendNotification);
    compareAButton.repaint();
    compareBButton.repaint();
    bypassButton.repaint();
}

//==============================================================================
void ParticleDelayAudioProcessorEditor::refreshPresetList()
{
    const int previous = presetBox.getSelectedId();
    presetBox.clear (juce::dontSendNotification);

    auto& pm = proc.presetManager;
    for (int i = 0; i < pm.getNumFactoryPresets(); ++i)
        presetBox.addItem (pm.getFactoryPresetName (i), i + 1);

    userPresetNames = pm.getUserPresetNames();
    if (! userPresetNames.isEmpty())
    {
        presetBox.addSeparator();
        for (int i = 0; i < userPresetNames.size(); ++i)
            presetBox.addItem (userPresetNames[i], userPresetIdBase + i);
    }

    if (previous > 0)
        presetBox.setSelectedId (previous, juce::dontSendNotification);
}

void ParticleDelayAudioProcessorEditor::applySelectedPreset()
{
    const int id = presetBox.getSelectedId();
    if (id <= 0)
        return;

    if (id < userPresetIdBase)
    {
        proc.setCurrentProgram (id - 1); // factory; routes through the program interface
    }
    else
    {
        const int userIndex = id - userPresetIdBase;
        if (userIndex >= 0 && userIndex < userPresetNames.size())
            proc.presetManager.loadUserPreset (userPresetNames[userIndex]);
    }
}

void ParticleDelayAudioProcessorEditor::stepPreset (int delta)
{
    // Ordered selectable ids: factory 1..N, then user presets.
    juce::Array<int> ids;
    for (int i = 0; i < proc.presetManager.getNumFactoryPresets(); ++i)
        ids.add (i + 1);
    for (int i = 0; i < userPresetNames.size(); ++i)
        ids.add (userPresetIdBase + i);

    if (ids.isEmpty())
        return;

    int pos = ids.indexOf (presetBox.getSelectedId());
    if (pos < 0)
        pos = (delta > 0 ? -1 : 0); // step into the list from "nothing selected"

    pos = juce::jlimit (0, ids.size() - 1, pos + delta);
    presetBox.setSelectedId (ids[pos], juce::sendNotificationSync);
}

void ParticleDelayAudioProcessorEditor::promptSaveUserPreset()
{
    auto* w = new juce::AlertWindow ("Save Preset", "Name this preset:",
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "My Preset");
    w->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, w] (int result)
        {
            if (result == 1)
            {
                const auto name = w->getTextEditorContents ("name").trim();
                if (name.isNotEmpty() && proc.presetManager.saveUserPreset (name))
                {
                    refreshPresetList();
                    const int idx = userPresetNames.indexOf (name);
                    if (idx >= 0)
                        presetBox.setSelectedId (userPresetIdBase + idx, juce::dontSendNotification);
                }
            }
        }),
        true); // deleteWhenDismissed
}

//==============================================================================
HelpPanel::Body::Body()
{
    text.setJustification (juce::Justification::topLeft);
    text.setLineSpacing (5.0f);

    const auto title = [&] (const juce::String& s)
    {
        text.append (s + "\n", ParticleLookAndFeel::displayFont (24.0f, juce::Font::bold),
                     ParticlePalette::textPrimary);
    };
    const auto para = [&] (const juce::String& s)
    {
        text.append (s + "\n", ParticleLookAndFeel::monoFont (13.5f),
                     ParticlePalette::textPrimary);
    };
    const auto section = [&] (const juce::String& s)
    {
        text.append ("\n" + s + "\n", ParticleLookAndFeel::monoFont (14.5f, juce::Font::bold),
                     ParticlePalette::accentCyan.withAlpha (0.82f));
    };
    const auto param = [&] (const juce::String& name, const juce::String& desc)
    {
        text.append (name + "  *  ", ParticleLookAndFeel::monoFont (13.5f, juce::Font::bold),
                     ParticlePalette::textPrimary);
        text.append (desc + "\n", ParticleLookAndFeel::monoFont (13.0f),
                     ParticlePalette::textDim);
    };

    title ("Particle Delay");
    para ("Particle Delay detects transients in the incoming audio. Each hit launches a "
          "burst of virtual particles that fall under gravity and bounce on a floor, losing "
          "energy each time. Every bounce replays that captured hit: the particle's "
          "horizontal position sets stereo pan, its energy sets level, and its impact speed "
          "sets brightness. The echoes accelerate and scatter instead of repeating on a "
          "fixed grid.");

    section ("PHYSICS");
    param ("Particles", "How many particles each detected hit launches (1-32).");
    param ("Gravity", "Fall speed and first-impact time, free-running and independent of tempo (~250 ms at 1.0x).");
    param ("Bounce", "How much speed and energy a particle keeps after each floor hit.");
    param ("Scatter", "Stereo spread and how widely particle release times are staggered.");
    param ("Decay", "How quickly particle energy - and therefore echo level - fades.");

    section ("CAPTURE");
    param ("Threshold", "Input level needed to trigger a hit (linear amplitude, not dB).");
    param ("Capture Length", "Maximum stereo audio stored per hit (80-500 ms).");
    param ("Smoothness", "Attack and release fades applied to every replayed hit.");

    section ("DELAY");
    param ("Delay Min", "Start of the audible bounce window; earlier bounces stay silent. Sync to tempo with the button.");
    param ("Delay Max", "End of the audible bounce window; later bounces stay silent. Sync to tempo with the button.");

    section ("OUTPUT");
    param ("Mix", "Balance between the dry input and the echoes.");
    param ("Output", "Final output level, for matching against the bypassed signal.");

    section ("SPACE  -  wet finishing");
    param ("Feedback", "Extends echo-train life past the Bounce/Decay limits for long, sustaining tails. Always fades eventually.");
    param ("High Pass", "Removes low frequencies from the echoes only.");
    param ("Low Pass", "Rolls off high frequencies from the echoes for a darker tail.");
    param ("Diffuse", "Smears sharp echoes into a softer, more reverberant wash.");
    param ("Size", "Length and spread of the diffusion (active when Diffuse is up).");
    param ("Width", "Stereo width of the echoes (0% mono, 100% normal, 200% wide).");
}

int HelpPanel::Body::idealHeightForWidth (int width) const
{
    juce::TextLayout layout;
    layout.createLayout (text, (float) juce::jmax (1, width));
    return (int) std::ceil (layout.getHeight());
}

void HelpPanel::Body::paint (juce::Graphics& g)
{
    juce::TextLayout layout;
    layout.createLayout (text, (float) getWidth());
    layout.draw (g, getLocalBounds().toFloat());
}

//==============================================================================
HelpPanel::HelpPanel()
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&body, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (9);

    closeButton.setButtonText ("CLOSE");
    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton.setColour (juce::TextButton::textColourOffId, ParticlePalette::accentCyan);
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);
}

HelpPanel::~HelpPanel() = default;

juce::Rectangle<int> HelpPanel::cardBounds() const
{
    return getLocalBounds().reduced (34, 44);
}

void HelpPanel::paint (juce::Graphics& g)
{
    using namespace ParticlePalette;

    g.fillAll (juce::Colours::black.withAlpha (0.72f));

    const auto card = cardBounds();
    auto cf = card.toFloat();
    g.setGradientFill (juce::ColourGradient (shellTop.brighter (0.05f), cf.getCentreX(), cf.getY(),
                                             shellBottom, cf.getCentreX(), cf.getBottom(), false));
    g.fillRoundedRectangle (cf, 5.0f);

    for (int y = card.getY(); y < card.getBottom(); y += 3)
    {
        g.setColour (accentCyan.withAlpha (0.025f));
        g.drawHorizontalLine (y + 2, (float) card.getX(), (float) card.getRight());
    }

    g.setColour (accentCyan.withAlpha (0.18f));
    g.drawRoundedRectangle (cf.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.70f));
    g.drawRoundedRectangle (cf.reduced (1.5f), 4.0f, 1.0f);

    auto header = card.reduced (20, 0).withHeight (49);
    g.setColour (accentCyan.withAlpha (0.12f));
    g.drawHorizontalLine (header.getBottom(), (float) card.getX() + 1.0f, (float) card.getRight() - 1.0f);

    g.setFont (ParticleLookAndFeel::displayFont (19.0f, juce::Font::bold));
    g.setColour (textPrimary);
    g.drawText ("PARAMETER GUIDE", header.removeFromLeft (220), juce::Justification::centredLeft, false);

    g.setFont (ParticleLookAndFeel::monoFont (8.5f));
    g.setColour (textMuted.brighter (0.65f));
    g.drawText ("PARTICLE DELAY  *  CONTROL REFERENCE", header.removeFromLeft (260),
                juce::Justification::centredLeft, false);
}

void HelpPanel::resized()
{
    auto inner = cardBounds().reduced (20, 0);

    auto top = inner.removeFromTop (49);
    closeButton.setBounds (top.removeFromRight (86).withSizeKeepingCentre (76, 28));
    inner.removeFromTop (18);
    inner.removeFromBottom (18);
    inner = inner.withTrimmedLeft (8).withTrimmedRight (8);

    viewport.setBounds (inner);

    // Reserve room for the scrollbar so wrapped text never sits under it.
    const int contentWidth = juce::jmax (1, viewport.getWidth() - 14);
    body.setSize (contentWidth, body.idealHeightForWidth (contentWidth));
}

void HelpPanel::mouseDown (const juce::MouseEvent& e)
{
    // A click on the dimmed backdrop (outside the card) closes the panel.
    if (! cardBounds().contains (e.getPosition()) && onClose)
        onClose();
}
