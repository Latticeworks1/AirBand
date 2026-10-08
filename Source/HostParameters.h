#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AirBandSettings.h"

// The host-facing parameter set and its conversion to DSP units (percent to 0-1 fractions).
namespace host
{
    inline constexpr auto midAirId = "midAir";
    inline constexpr auto highAirId = "highAir";
    inline constexpr auto blendId = "blend";
    inline constexpr auto outputId = "output";
    inline constexpr auto deEssId = "deEss";
    inline constexpr auto compId = "comp";
    inline constexpr auto gateId = "gate";
    inline constexpr auto gateThresholdId = "gateThreshold";
    inline constexpr auto limiterId = "limiter";
    inline constexpr auto levelTrackingId = "levelTracking";

    // Defaults come from AirBandSettings{}.
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    AirBandSettings readSettings (const juce::AudioProcessorValueTreeState& state);
}
