#include "PluginProcessor.h"
#include "PluginEditor.h"

AirBandAudioProcessor::AirBandAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AirBandAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { midAirId, 1 }, "Mid Air",
        juce::NormalisableRange<float> (0.0f, 15.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { highAirId, 1 }, "High Air",
        juce::NormalisableRange<float> (0.0f, 15.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { blendId, 1 }, "Air Blend",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 20.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputId, 1 }, "Output",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { deEssId, 1 }, "De-Ess",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { compId, 1 }, "Comp",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gateId, 1 }, "Gate",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { limiterId, 1 }, "Limiter",
        juce::NormalisableRange<float> (-12.0f, 0.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

void AirBandAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());

    // The output limiter uses lookahead, which delays the signal; report
    // it so the host applies plugin delay compensation instead of AirBand
    // silently drifting out of sync with unprocessed tracks.
    setLatencySamples (dsp.getLatencySamples());
}

void AirBandAudioProcessor::releaseResources()
{
    dsp.reset();
}

bool AirBandAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainIn == mainOut;
}

void AirBandAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float midBoost = apvts.getRawParameterValue (midAirId)->load();
    const float highBoost = apvts.getRawParameterValue (highAirId)->load();
    const float blend = apvts.getRawParameterValue (blendId)->load() / 100.0f;
    const float output = apvts.getRawParameterValue (outputId)->load();
    const float deEss = apvts.getRawParameterValue (deEssId)->load() / 100.0f;
    const float comp = apvts.getRawParameterValue (compId)->load() / 100.0f;
    const float gateAmount = apvts.getRawParameterValue (gateId)->load() / 100.0f;
    const float limiterCeiling = apvts.getRawParameterValue (limiterId)->load();

    dsp.setParameters (midBoost, highBoost, blend, output, deEss, comp, gateAmount, limiterCeiling);
    dsp.processBlock (buffer);
}

juce::AudioProcessorEditor* AirBandAudioProcessor::createEditor()
{
    return new AirBandAudioProcessorEditor (*this);
}

void AirBandAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); true)
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void AirBandAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AirBandAudioProcessor();
}
