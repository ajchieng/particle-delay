cmake_minimum_required(VERSION 3.25)

project(ParticleDelay VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE lives in ./JUCE (symlink to a shallow clone of tag 8.0.13). Override with
# -DJUCE_PATH=/path/to/JUCE if you keep JUCE somewhere else.
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to the JUCE framework")
add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(ParticleDelay
    COMPANY_NAME            "Chieng"
    PRODUCT_NAME            "Particle Delay"
    BUNDLE_ID               com.chieng.particledelay
    PLUGIN_MANUFACTURER_CODE Chng     # 4 chars, >=1 uppercase
    PLUGIN_CODE             Ptd1       # 4 chars, >=1 uppercase
    FORMATS                 AU VST3 Standalone  # AU for Logic; VST3 for Cubase/Reaper/Windows; Standalone for quick testing
    VST3_CATEGORIES         "Fx" "Delay"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)       # auto-installs to ~/Library/Audio/Plug-Ins/Components

juce_generate_juce_header(ParticleDelay)

target_sources(ParticleDelay PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ParticleLookAndFeel.cpp
    Source/PresetManager.cpp
    Source/DSP/CapturedHit.cpp
    Source/DSP/TransientDetector.cpp
    Source/DSP/ParticleSystem.cpp
    Source/DSP/WetFinisher.cpp)

target_include_directories(ParticleDelay PRIVATE Source)

target_compile_definitions(ParticleDelay PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ParticleDelay
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

#==============================================================================
# Optional offline DSP test harness (no DAW needed):
#   cmake -B build -G Xcode -DPARTICLEDELAY_BUILD_TESTS=ON
#   cmake --build build --config Release --target ParticleDelayTests
#   ./build/ParticleDelayTests_artefacts/Release/ParticleDelayTests
option(PARTICLEDELAY_BUILD_TESTS "Build the offline DSP test harness" OFF)

if(PARTICLEDELAY_BUILD_TESTS)
    juce_add_console_app(ParticleDelayTests PRODUCT_NAME "ParticleDelayTests")
    juce_generate_juce_header(ParticleDelayTests)

    target_sources(ParticleDelayTests PRIVATE
        Tests/DspTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ParticleLookAndFeel.cpp
        Source/PresetManager.cpp
        Source/DSP/CapturedHit.cpp
        Source/DSP/DelayBuffer.cpp
        Source/DSP/TransientDetector.cpp
        Source/DSP/ParticleSystem.cpp
        Source/DSP/WetFinisher.cpp)

    target_compile_definitions(ParticleDelayTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        PARTICLEDELAY_ENABLE_TEST_HOOKS=1
        JucePlugin_Name="Particle Delay")

    target_link_libraries(ParticleDelayTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
