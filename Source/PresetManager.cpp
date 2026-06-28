#include "PresetManager.h"
#include <array>
#include <utility>

PresetManager::PresetManager (juce::AudioProcessor& processor,
                              juce::AudioProcessorValueTreeState& state)
    : proc (processor), apvts (state)
{
}

//==============================================================================
const std::vector<PresetManager::FactoryPreset>& PresetManager::factoryPresets()
{
    // Values are plain parameter values; applyFactoryPreset converts to 0..1.
    static const std::vector<FactoryPreset> presets {
        { "Default", {} },
        { "Subtle Scatter", {
            { "MIX", 0.25f }, { "PARTICLES", 5.0f }, { "GRAVITY", 0.75f },
            { "BOUNCE", 0.65f }, { "SCATTER", 0.30f }, { "DECAY", 0.990f },
            { "DELAY_MIN_MS", 60.0f }, { "DELAY_MAX_MS", 700.0f } } },
        { "Bouncing Ball", {
            { "MIX", 0.40f }, { "PARTICLES", 8.0f }, { "GRAVITY", 1.0f },
            { "BOUNCE", 0.82f }, { "SCATTER", 0.45f }, { "DECAY", 0.995f },
            { "DELAY_MIN_MS", 40.0f }, { "DELAY_MAX_MS", 1200.0f } } },
        { "Chaos Cloud", {
            { "MIX", 0.65f }, { "PARTICLES", 20.0f }, { "GRAVITY", 3.0f },
            { "BOUNCE", 0.92f }, { "SCATTER", 0.90f }, { "DECAY", 0.998f },
            { "DELAY_MIN_MS", 20.0f }, { "DELAY_MAX_MS", 1800.0f } } },
        { "Lush", {
            { "MIX", 0.45f }, { "PARTICLES", 10.0f }, { "GRAVITY", 0.90f },
            { "BOUNCE", 0.80f }, { "SCATTER", 0.60f }, { "DECAY", 0.996f },
            { "SMOOTHNESS", 0.60f }, { "DELAY_MIN_MS", 80.0f }, { "DELAY_MAX_MS", 1500.0f },
            { "FEEDBACK", 0.50f }, { "WET_LP", 9000.0f }, { "DIFFUSE", 0.55f },
            { "DIFFUSE_SIZE", 0.60f }, { "WET_WIDTH", 1.40f } } },
        { "Drum Tight Slap", {
            { "MIX", 0.22f }, { "PARTICLES", 4.0f }, { "GRAVITY", 2.50f },
            { "BOUNCE", 0.55f }, { "SCATTER", 0.20f }, { "DECAY", 0.985f },
            { "CAPTURE_MAX_MS", 120.0f }, { "SMOOTHNESS", 0.25f },
            { "DELAY_MIN_MS", 35.0f }, { "DELAY_MAX_MS", 260.0f },
            { "THRESHOLD", 0.18f }, { "WET_HP", 80.0f }, { "WET_LP", 12000.0f },
            { "WET_WIDTH", 1.10f } } },
        { "Drum Room Scatter", {
            { "MIX", 0.32f }, { "PARTICLES", 9.0f }, { "GRAVITY", 1.40f },
            { "BOUNCE", 0.68f }, { "SCATTER", 0.55f }, { "DECAY", 0.992f },
            { "CAPTURE_MAX_MS", 160.0f }, { "SMOOTHNESS", 0.45f },
            { "DELAY_MIN_MS", 45.0f }, { "DELAY_MAX_MS", 700.0f },
            { "THRESHOLD", 0.16f }, { "WET_HP", 90.0f }, { "WET_LP", 10000.0f },
            { "DIFFUSE", 0.20f }, { "DIFFUSE_SIZE", 0.35f }, { "WET_WIDTH", 1.25f } } },
        { "Snare Ghosts", {
            { "MIX", 0.38f }, { "PARTICLES", 6.0f }, { "GRAVITY", 0.80f },
            { "BOUNCE", 0.75f }, { "SCATTER", 0.35f }, { "DECAY", 0.994f },
            { "CAPTURE_MAX_MS", 180.0f }, { "SMOOTHNESS", 0.35f },
            { "DELAY_MIN_MS", 90.0f }, { "DELAY_MAX_MS", 950.0f },
            { "THRESHOLD", 0.15f }, { "WET_HP", 110.0f }, { "WET_LP", 8500.0f },
            { "WET_WIDTH", 1.15f } } },
        { "Vocal Doubler Cloud", {
            { "MIX", 0.18f }, { "PARTICLES", 4.0f }, { "GRAVITY", 0.65f },
            { "BOUNCE", 0.60f }, { "SCATTER", 0.30f }, { "DECAY", 0.990f },
            { "CAPTURE_MAX_MS", 220.0f }, { "SMOOTHNESS", 0.75f },
            { "DELAY_MIN_MS", 70.0f }, { "DELAY_MAX_MS", 450.0f },
            { "THRESHOLD", 0.08f }, { "WET_HP", 160.0f }, { "WET_LP", 12000.0f },
            { "DIFFUSE", 0.25f }, { "DIFFUSE_SIZE", 0.45f }, { "WET_WIDTH", 1.35f } } },
        { "Vocal Throw", {
            { "MIX", 0.42f }, { "PARTICLES", 7.0f }, { "GRAVITY", 0.45f },
            { "BOUNCE", 0.78f }, { "SCATTER", 0.45f }, { "DECAY", 0.996f },
            { "CAPTURE_MAX_MS", 300.0f }, { "SMOOTHNESS", 0.65f },
            { "DELAY_MIN_MS", 180.0f }, { "DELAY_MAX_MS", 1600.0f },
            { "THRESHOLD", 0.10f }, { "FEEDBACK", 0.35f }, { "WET_HP", 140.0f },
            { "WET_LP", 7500.0f }, { "DIFFUSE", 0.20f }, { "WET_WIDTH", 1.20f } } },
        { "Piano Sparkle", {
            { "MIX", 0.28f }, { "PARTICLES", 8.0f }, { "GRAVITY", 1.15f },
            { "BOUNCE", 0.70f }, { "SCATTER", 0.50f }, { "DECAY", 0.993f },
            { "CAPTURE_MAX_MS", 250.0f }, { "SMOOTHNESS", 0.40f },
            { "DELAY_MIN_MS", 80.0f }, { "DELAY_MAX_MS", 1000.0f },
            { "THRESHOLD", 0.10f }, { "WET_HP", 120.0f }, { "WET_LP", 14000.0f },
            { "DIFFUSE", 0.15f }, { "WET_WIDTH", 1.30f } } },
        { "Guitar Halo", {
            { "MIX", 0.25f }, { "PARTICLES", 7.0f }, { "GRAVITY", 0.70f },
            { "BOUNCE", 0.73f }, { "SCATTER", 0.45f }, { "DECAY", 0.995f },
            { "CAPTURE_MAX_MS", 220.0f }, { "SMOOTHNESS", 0.55f },
            { "DELAY_MIN_MS", 110.0f }, { "DELAY_MAX_MS", 1300.0f },
            { "THRESHOLD", 0.09f }, { "WET_HP", 120.0f }, { "WET_LP", 9000.0f },
            { "DIFFUSE", 0.35f }, { "DIFFUSE_SIZE", 0.55f }, { "WET_WIDTH", 1.25f } } },
        { "Dub Send", {
            { "MIX", 1.0f }, { "PARTICLES", 5.0f }, { "GRAVITY", 0.55f },
            { "BOUNCE", 0.85f }, { "SCATTER", 0.40f }, { "DECAY", 0.997f },
            { "CAPTURE_MAX_MS", 260.0f }, { "SMOOTHNESS", 0.55f },
            { "DELAY_MIN_MS", 220.0f }, { "DELAY_MAX_MS", 2400.0f },
            { "THRESHOLD", 0.12f }, { "FEEDBACK", 0.65f }, { "WET_HP", 150.0f },
            { "WET_LP", 5500.0f }, { "DIFFUSE", 0.25f }, { "WET_WIDTH", 1.15f } } },
        { "Ambient Wash", {
            { "MIX", 0.55f }, { "PARTICLES", 14.0f }, { "GRAVITY", 0.25f },
            { "BOUNCE", 0.88f }, { "SCATTER", 0.75f }, { "DECAY", 0.998f },
            { "CAPTURE_MAX_MS", 420.0f }, { "SMOOTHNESS", 0.85f },
            { "DELAY_MIN_MS", 250.0f }, { "DELAY_MAX_MS", 6000.0f },
            { "THRESHOLD", 0.07f }, { "FEEDBACK", 0.60f }, { "WET_HP", 180.0f },
            { "WET_LP", 7000.0f }, { "DIFFUSE", 0.75f }, { "DIFFUSE_SIZE", 0.85f },
            { "WET_WIDTH", 1.50f } } },
        { "Bass Safe Space", {
            { "MIX", 0.20f }, { "PARTICLES", 5.0f }, { "GRAVITY", 0.90f },
            { "BOUNCE", 0.64f }, { "SCATTER", 0.25f }, { "DECAY", 0.990f },
            { "CAPTURE_MAX_MS", 160.0f }, { "SMOOTHNESS", 0.50f },
            { "DELAY_MIN_MS", 70.0f }, { "DELAY_MAX_MS", 650.0f },
            { "THRESHOLD", 0.16f }, { "FEEDBACK", 0.15f }, { "WET_HP", 220.0f },
            { "WET_LP", 8000.0f }, { "DIFFUSE", 0.20f }, { "WET_WIDTH", 1.0f } } },
        { "Wide Micro Scatter", {
            { "MIX", 0.12f }, { "PARTICLES", 6.0f }, { "GRAVITY", 1.80f },
            { "BOUNCE", 0.45f }, { "SCATTER", 0.75f }, { "DECAY", 0.985f },
            { "CAPTURE_MAX_MS", 100.0f }, { "SMOOTHNESS", 0.30f },
            { "DELAY_MIN_MS", 20.0f }, { "DELAY_MAX_MS", 220.0f },
            { "THRESHOLD", 0.12f }, { "WET_HP", 120.0f }, { "WET_LP", 16000.0f },
            { "DIFFUSE", 0.10f }, { "WET_WIDTH", 1.70f } } },
    };
    return presets;
}

int PresetManager::getNumFactoryPresets() const
{
    return (int) factoryPresets().size();
}

juce::String PresetManager::getFactoryPresetName (int index) const
{
    const auto& presets = factoryPresets();
    if (index >= 0 && index < (int) presets.size())
        return presets[(size_t) index].name;
    return {};
}

void PresetManager::resetAllToDefaults()
{
    for (auto* param : proc.getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());
}

void PresetManager::applyFactoryPreset (int index)
{
    const auto& presets = factoryPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    resetAllToDefaults();
    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
}

//==============================================================================
juce::File PresetManager::presetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Chieng")
        .getChildFile ("Particle Delay")
        .getChildFile ("Presets");
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    const auto dir = presetDirectory();
    if (dir.isDirectory())
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
            names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty())
        return false;

    auto dir = presetDirectory();
    dir.createDirectory();
    auto file = dir.getChildFile (juce::File::createLegalFileName (trimmed) + ".xml");

    if (auto xml = apvts.copyState().createXml())
        return file.replaceWithText (xml->toString());
    return false;
}

bool PresetManager::loadUserPreset (const juce::String& name)
{
    auto file = presetDirectory().getChildFile (juce::File::createLegalFileName (name) + ".xml");
    if (! file.existsAsFile())
        return false;

    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            migrateState (tree);
            apvts.replaceState (tree);
            return true;
        }
    return false;
}

//==============================================================================
void PresetManager::migrateState (juce::ValueTree& state)
{
    // GRAVITY was added after the first release; default it to 1.0x.
    if (! state.hasProperty ("GRAVITY"))
        state.setProperty ("GRAVITY", 1.0f, nullptr);

    if (! state.hasProperty ("TIMING_MODE"))
        state.setProperty ("TIMING_MODE", 0, nullptr);

    if (! state.hasProperty ("TIMING_DIV"))
        state.setProperty ("TIMING_DIV", 9, nullptr);

    // The SPACE panel parameters were added later; default any that a pre-SPACE
    // state is missing to their neutral values so the stage stays transparent.
    const std::array<std::pair<const char*, float>, 6> spaceDefaults {{
        { "FEEDBACK",     0.0f     },
        { "WET_HP",       20.0f    },
        { "WET_LP",       20000.0f },
        { "DIFFUSE",      0.0f     },
        { "DIFFUSE_SIZE", 0.5f     },
        { "WET_WIDTH",    1.0f     },
    }};
    for (const auto& [id, value] : spaceDefaults)
        if (! state.hasProperty (id))
            state.setProperty (id, value, nullptr);

    if (! state.hasProperty ("BYPASS"))
        state.setProperty ("BYPASS", false, nullptr);
}
