#pragma once

#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include "HostParameters.h"

class AirBandDSP;

class AirBandAudioProcessor : public juce::AudioProcessor
{
public:
    AirBandAudioProcessor();
    ~AirBandAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static constexpr auto midAirId = host::midAirId;
    static constexpr auto highAirId = host::highAirId;
    static constexpr auto blendId = host::blendId;
    static constexpr auto outputId = host::outputId;
    static constexpr auto deEssId = host::deEssId;
    static constexpr auto compId = host::compId;
    static constexpr auto gateId = host::gateId;
    static constexpr auto limiterId = host::limiterId;

private:
    std::unique_ptr<AirBandDSP> dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirBandAudioProcessor)
};
