#include "HostParameters.h"

namespace host
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    const AirBandSettings defaults;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { host::midAirId, 1 }, "Mid Air",
        juce::NormalisableRange<float> (0.0f, 15.0f, 0.01f), defaults.midAirDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { host::highAirId, 1 }, "High Air",
        juce::NormalisableRange<float> (0.0f, 15.0f, 0.01f), defaults.highAirDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { host::blendId, 1 }, "Air Blend",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaults.blend * 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { host::outputId, 1 }, "Output",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), defaults.outputDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { host::deEssId, 1 }, "De-Ess",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaults.deEssAmount * 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { host::compId, 1 }, "Comp",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaults.compAmount * 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { host::gateId, 1 }, "Gate",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaults.gateAmount * 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { host::limiterId, 1 }, "Limiter",
        juce::NormalisableRange<float> (-12.0f, 0.0f, 0.01f), defaults.limiterCeilingDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

AirBandSettings readSettings (const juce::AudioProcessorValueTreeState& state)
{
    const auto value = [&state] (const char* id) { return state.getRawParameterValue (id)->load(); };

    return { .midAirDb = value (midAirId),
             .highAirDb = value (highAirId),
             .blend = value (blendId) / 100.0f,
             .outputDb = value (outputId),
             .deEssAmount = value (deEssId) / 100.0f,
             .compAmount = value (compId) / 100.0f,
             .gateAmount = value (gateId) / 100.0f,
             .limiterCeilingDb = value (limiterId) };
}
}
