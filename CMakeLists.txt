cmake_minimum_required(VERSION 3.22)

# Link the MSVC runtime statically so the Windows VST3 loads on a machine that
# has never installed the Visual C++ Redistributable. Must precede project().
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(AirBand VERSION 1.3.2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Behaviour is specified by bit-exact regression vectors, so the default build is the optimised one.
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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

# The signal processing. The plugin and the tests link this one target, so both run the same
# object code under the same compiler flags.
add_library(airband_core STATIC
    Source/AirBand.cpp
    Source/AirBandDSP.cpp
    Source/VocalCompressor.cpp
    Source/VocalGate.cpp
    Source/VocalLimiter.cpp
)

target_include_directories(airband_core PUBLIC Source)

# RealtimeSanitizer (LLVM 19+; Apple clang does not ship the runtime): instruments the functions
# marked AIRBAND_NONBLOCKING. Used only by scripts/check_realtime.sh, in its own build tree.
option(AIRBAND_RTSAN "Build the core and tests with -fsanitize=realtime" OFF)
if (AIRBAND_RTSAN)
    target_compile_options(airband_core PUBLIC -fsanitize=realtime)
    target_link_options(airband_core PUBLIC -fsanitize=realtime)
endif()

target_compile_definitions(airband_core
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(airband_core
    PUBLIC
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_sources(AirBand
    PRIVATE
        Source/HostParameters.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(AirBand
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(AirBand
    PRIVATE
        airband_core
        juce::juce_audio_utils
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
            Tests/TestMain.cpp
            Tests/Check.cpp
            Tests/TestSignals.cpp
            Tests/TestMetrics.cpp
            Tests/Harness.cpp
            Tests/LinearTests.cpp
            Tests/AirTests.cpp
            Tests/DynamicsTests.cpp
            Tests/TrackingTests.cpp
            Tests/TransportTests.cpp
            Tests/PartitionTests.cpp
            Tests/SafetyTests.cpp
            Tests/RealtimeTests.cpp
            Tests/GoldenCases.cpp
            Tests/GoldenTests.cpp
    )

    target_include_directories(AirBand_Tests PRIVATE Tests)
    target_link_libraries(AirBand_Tests PRIVATE airband_core)

    # The host boundary: parameter layout and conversion, through a real AudioProcessorValueTreeState.
    juce_add_console_app(AirBand_HostTests
        PRODUCT_NAME "AirBand Host Tests"
    )

    target_sources(AirBand_HostTests
        PRIVATE
            Tests/HostTests.cpp
            Tests/Check.cpp
            Source/HostParameters.cpp
    )

    target_include_directories(AirBand_HostTests PRIVATE Tests Source)

    target_compile_definitions(AirBand_HostTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

    target_link_libraries(AirBand_HostTests
        PRIVATE
            juce::juce_audio_processors
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    enable_testing()
    add_test(NAME AirBandDSPTests COMMAND AirBand_Tests)
    add_test(NAME AirBandHostTests COMMAND AirBand_HostTests)
endif()
