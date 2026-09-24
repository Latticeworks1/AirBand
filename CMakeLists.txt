cmake_minimum_required(VERSION 3.22)

project(AirBand VERSION 1.1.0)

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
        Source/VocalCompressor.cpp
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

# Pushes real synthetic audio through the actual DSP classes (no plugin
# hosting involved) and checks the output against expected physical
# behaviour, e.g. that a quiet high-frequency tone actually comes out
# louder and a loud one doesn't. This exists because a test suite that
# only checks "did it compile" / "did setParameters throw" can pass while
# the signal path itself is silently broken (wrong wiring, a stage that
# never runs, output pinned to zero); only running real numbers through
# processBlock and inspecting the waveform catches that class of bug.
option(AIRBAND_BUILD_TESTS "Build the AirBand_Tests DSP test executable" ON)

if (AIRBAND_BUILD_TESTS)
    juce_add_console_app(AirBand_Tests
        PRODUCT_NAME "AirBand Tests"
    )

    target_sources(AirBand_Tests
        PRIVATE
            Tests/AirBandDSPTests.cpp
            Source/AirBandDSP.cpp
            Source/VocalCompressor.cpp
    )

    target_include_directories(AirBand_Tests PRIVATE Source)

    target_compile_definitions(AirBand_Tests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )

    target_link_libraries(AirBand_Tests
        PRIVATE
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    enable_testing()
    add_test(NAME AirBandDSPTests COMMAND AirBand_Tests)
endif()
