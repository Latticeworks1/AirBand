#include "PluginProcessor.h"
#include "AirBandDSP.h"
#include "HostParameters.h"
#include "PluginEditor.h"

AirBandAudioProcessor::AirBandAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", host::createLayout()),
      dsp (std::make_unique<AirBandDSP>())
{
}

AirBandAudioProcessor::~AirBandAudioProcessor() = default;

void AirBandAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp->prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());

    // The output limiter uses lookahead, which delays the signal; report
    // it so the host applies plugin delay compensation instead of AirBand
    // silently drifting out of sync with unprocessed tracks.
    setLatencySamples (dsp->getLatencySamples());
}

void AirBandAudioProcessor::releaseResources()
{
    dsp->reset();
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

    dsp->setParameters (host::readSettings (apvts));
    dsp->processBlock (buffer);
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
