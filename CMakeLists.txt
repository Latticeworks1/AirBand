cmake_minimum_required(VERSION 3.22)

project(AirBand VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dev builds install straight into ~/Library/Audio/Plug-Ins for fast iteration;
# distribution builds turn this off and package the artefacts instead, so a
# release build never silently overwrites whatever the user is testing.
option(AIRBAND_COPY_AFTER_BUILD "Copy plugin into the system plugin folders after building" ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AirBand
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "AirBand"
    BUNDLE_ID "com.airband.airband"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${AIRBAND_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE Abnd
    PLUGIN_CODE Arb1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "AirBand"
)

target_sources(AirBand
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/AirBandDSP.cpp
)

target_compile_definitions(AirBand
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(AirBand
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
