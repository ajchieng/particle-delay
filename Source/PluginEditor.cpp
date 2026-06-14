#include "PluginEditor.h"

namespace
{
    // Knob order, grouped by section so layout is index-contiguous:
    //   [0..4] PHYSICS  [5..7] CAPTURE  [8..9] DELAY  [10..11] OUTPUT  [12..17] SPACE
    struct KnobDef { const char* id; const char* name; const char* tip; };
    const std::array<KnobDef, 18> knobDefs {{
        { "PARTICLES",     "Particles",      "Particles launched per detected hit (1-32)." },                // 0  PHYSICS
        { "GRAVITY",       "Gravity",        "Fall speed and first-impact time; free-running, not tempo-synced." }, // 1
        { "BOUNCE",        "Bounce",         "Energy and speed a particle keeps after each floor hit." },     // 2
        { "SCATTER",       "Scatter",        "Stereo spread and release-time variation of the burst." },      // 3
        { "DECAY",         "Decay",          "How quickly particle energy (and echo level) fades." },         // 4
        { "THRESHOLD",     "Threshold",      "Input level needed to trigger a hit. Watch the meter below." }, // 5  CAPTURE
        { "CAPTURE_MAX_MS","Capture Length", "Maximum stereo audio captured per hit (80-500 ms)." },          // 6
        { "SMOOTHNESS",    "Smoothness",     "Attack and release fades applied to each replayed hit." },       // 7
        { "DELAY_MIN_MS",  "Delay Min",      "Start of the audible bounce window. Sync to tempo with the button." }, // 8  DELAY
        { "DELAY_MAX_MS",  "Delay Max",      "End of the audible bounce window. Sync to tempo with the button." },   // 9
        { "MIX",           "Mix",            "Balance between the dry input and the echoes." },               // 10 OUTPUT
        { "OUTPUT",        "Output",         "Final output level." },                                          // 11
        { "FEEDBACK",      "Feedback",       "Extends echo-train life past the Bounce/Decay limits." },        // 12 SPACE
        { "WET_HP",        "High Pass",      "High-pass filter on the echoes only." },                         // 13
        { "WET_LP",        "Low Pass",       "Low-pass filter on the echoes for a darker tail." },             // 14
        { "DIFFUSE",       "Diffuse",        "Smears sharp echoes into a softer, reverberant wash." },         // 15
        { "DIFFUSE_SIZE",  "Size",           "Length and spread of the diffusion (active when Diffuse is up)." }, // 16
        { "WET_WIDTH",     "Width",          "Stereo width of the echoes (0% mono, 200% wide)." },             // 17
    }};

    // Delay knobs carry an extra tempo-sync row; tracked by name, not a literal.
    constexpr int kThresholdKnob = 5;
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
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, ParticlePalette::accentCyan);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    resetButton.setColour (juce::TextButton::buttonColourId, ParticlePalette::panelFill);
    resetButton.setColour (juce::TextButton::textColourOffId, ParticlePalette::accentCyan);
    resetButton.setTooltip ("Reset all parameters to their defaults");
    resetButton.onClick = [this] { proc.resetToDefaults(); };
    addAndMakeVisible (resetButton);

    helpButton.setColour (juce::TextButton::buttonColourId, ParticlePalette::panelFill);
    helpButton.setColour (juce::TextButton::textColourOffId, ParticlePalette::accentCyan);
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
        b.setColour (juce::TextButton::buttonColourId, ParticlePalette::panelFill);
        b.setColour (juce::TextButton::textColourOffId, ParticlePalette::accentCyan);
        b.setTooltip (tip);
        addAndMakeVisible (b);
    };
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setTooltip ("Load a factory or user preset");
    presetBox.onChange = [this] { applySelectedPreset(); };
    addAndMakeVisible (presetBox);

    prevPresetButton.onClick = [this] { stepPreset (-1); };
    nextPresetButton.onClick = [this] { stepPreset (1); };
    savePresetButton.onClick = [this] { promptSaveUserPreset(); };
    styleBarButton (prevPresetButton, "Previous preset");
    styleBarButton (nextPresetButton, "Next preset");
    styleBarButton (savePresetButton, "Save the current settings as a user preset");

    refreshPresetList();

    for (int i = 0; i < numKnobs; ++i)
        addKnob (knobs[(size_t) i], knobDefs[(size_t) i].id,
                 knobDefs[(size_t) i].name, knobDefs[(size_t) i].tip);

    addDelaySyncControl (delaySyncControls[0], "DELAY_MIN_SYNC", "DELAY_MIN_DIV");
    addDelaySyncControl (delaySyncControls[1], "DELAY_MAX_SYNC", "DELAY_MAX_DIV");

    sections[0].title = "PHYSICS";
    sections[1].title = "CAPTURE";
    sections[2].title = "DELAY";
    sections[3].title = "OUTPUT";
    sections[4].title = "SPACE";

    setSize (820, 847);
}

ParticleDelayAudioProcessorEditor::~ParticleDelayAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ParticleDelayAudioProcessorEditor::addKnob (Knob& knob,
                                                 const juce::String& paramID,
                                                 const juce::String& displayName,
                                                 const juce::String& tooltip)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setColour (juce::Slider::textBoxTextColourId,    ParticlePalette::textPrimary);
    knob.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.slider.setTooltip (tooltip);
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
        else if (index == kThresholdKnob)
        {
            // Reserve a thin strip below the knob for the input/threshold meter.
            thresholdMeter.setBounds (cell.removeFromBottom (8));
            cell.removeFromBottom (4);
        }

        knobs[(size_t) index].slider.setBounds (cell);
    }
}

void ParticleDelayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (30);
    helpButton.setBounds (header.removeFromRight (30));
    header.removeFromRight (8);
    resetButton.setBounds (header.removeFromRight (64));
    header.removeFromRight (8);
    titleLabel.setBounds (header);
    area.removeFromTop (10); // headroom for the accent rule drawn in paint()

    auto presetBar = area.removeFromTop (26);
    prevPresetButton.setBounds (presetBar.removeFromLeft (28));
    presetBar.removeFromLeft (4);
    savePresetButton.setBounds (presetBar.removeFromRight (60));
    presetBar.removeFromRight (4);
    nextPresetButton.setBounds (presetBar.removeFromRight (28));
    presetBar.removeFromRight (6);
    presetBox.setBounds (presetBar);
    area.removeFromTop (10);

    particleView.setBounds (area.removeFromTop (180));
    area.removeFromTop (14);

    const int gap = 10;

    // Row A: PHYSICS spans the full width.
    sections[0].bounds = area.removeFromTop (175);
    area.removeFromTop (gap);

    // Row B: CAPTURE | DELAY | OUTPUT, widths weighted by knob count (3:2:2).
    auto rowB = area.removeFromTop (175);
    area.removeFromTop (gap);
    const int totalW   = rowB.getWidth() - 2 * gap;
    const int wCapture = totalW * 3 / 7;
    const int wDelay   = totalW * 2 / 7;

    sections[1].bounds = rowB.removeFromLeft (wCapture);
    rowB.removeFromLeft (gap);
    sections[2].bounds = rowB.removeFromLeft (wDelay);
    rowB.removeFromLeft (gap);
    sections[3].bounds = rowB; // OUTPUT takes the remainder

    // Row C: SPACE (the wet finishing stage) spans the full width.
    sections[4].bounds = area;

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
    layoutKnobRow (innerOf (sections[4].bounds), 12, 6); // SPACE

    helpPanel.setBounds (getLocalBounds());
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
    text.setLineSpacing (3.0f);

    const auto title = [&] (const juce::String& s)
    {
        text.append (s + "\n", juce::Font (juce::FontOptions (18.0f, juce::Font::bold)),
                     ParticlePalette::accentCyan);
    };
    const auto para = [&] (const juce::String& s)
    {
        text.append (s + "\n", juce::Font (juce::FontOptions (13.5f)),
                     ParticlePalette::textPrimary);
    };
    const auto section = [&] (const juce::String& s)
    {
        text.append ("\n" + s + "\n", juce::Font (juce::FontOptions (13.0f, juce::Font::bold)),
                     ParticlePalette::accentWarm);
    };
    const auto param = [&] (const juce::String& name, const juce::String& desc)
    {
        text.append (name + "  —  ", juce::Font (juce::FontOptions (13.0f, juce::Font::bold)),
                     ParticlePalette::textPrimary);
        text.append (desc + "\n", juce::Font (juce::FontOptions (13.0f)),
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

    closeButton.setColour (juce::TextButton::buttonColourId, ParticlePalette::panelFill);
    closeButton.setColour (juce::TextButton::textColourOffId, ParticlePalette::accentCyan);
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);
}

HelpPanel::~HelpPanel() = default;

juce::Rectangle<int> HelpPanel::cardBounds() const
{
    return getLocalBounds().reduced (juce::jmin (60, getWidth() / 8),
                                     juce::jmin (40, getHeight() / 10));
}

void HelpPanel::paint (juce::Graphics& g)
{
    // Dim the editor behind the panel, then draw the card.
    g.fillAll (juce::Colours::black.withAlpha (0.6f));

    const auto card = cardBounds();
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 24, { 0, 6 })
        .drawForRectangle (g, card);

    auto cf = card.toFloat();
    g.setColour (ParticlePalette::panelFill);
    g.fillRoundedRectangle (cf, 10.0f);
    g.setColour (ParticlePalette::accentCyan.withAlpha (0.4f));
    g.drawRoundedRectangle (cf, 10.0f, 1.5f);
}

void HelpPanel::resized()
{
    auto inner = cardBounds().reduced (18);

    auto top = inner.removeFromTop (28);
    closeButton.setBounds (top.removeFromRight (72));
    inner.removeFromTop (10);

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
