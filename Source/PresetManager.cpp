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
}
