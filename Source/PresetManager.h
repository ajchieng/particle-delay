#pragma once

#include <JuceHeader.h>
#include <map>
#include <vector>

//==============================================================================
// Factory presets (also surfaced through the host program interface) plus
// user-preset save/load to disk. Decoupled from the concrete processor: it only
// needs the AudioProcessor (for parameter enumeration) and its APVTS.
//
// Applying a factory preset first resets every parameter to its default, then
// applies the preset's overrides, so parameters a preset doesn't mention (e.g.
// the SPACE panel) return to neutral rather than keeping stale values.
class PresetManager
{
public:
    PresetManager (juce::AudioProcessor& processor,
                   juce::AudioProcessorValueTreeState& state);

    // ---- Factory presets ----------------------------------------------------
    int getNumFactoryPresets() const;
    juce::String getFactoryPresetName (int index) const;
    void applyFactoryPreset (int index);

    // ---- User presets (XML files on disk) -----------------------------------
    juce::StringArray getUserPresetNames() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);

    static juce::File presetDirectory();

    // Fill any parameters a loaded state/preset is missing with their defaults,
    // so older sessions and presets stay forward-compatible. Shared with the
    // processor's setStateInformation.
    static void migrateState (juce::ValueTree& state);

private:
    struct FactoryPreset
    {
        juce::String name;
        std::map<juce::String, float> values;   // parameter ID -> plain value
    };

    static const std::vector<FactoryPreset>& factoryPresets();

    void resetAllToDefaults();

    juce::AudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
