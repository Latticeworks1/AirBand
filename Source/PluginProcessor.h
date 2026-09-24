#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AirBandDSP.h"

class AirBandAudioProcessor : public juce::AudioProcessor
{
public:
    AirBandAudioProcessor();
    ~AirBandAudioProcessor() override = default;

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

    static constexpr auto midAirId = "midAir";
    static constexpr auto highAirId = "highAir";
    static constexpr auto blendId = "blend";
    static constexpr auto outputId = "output";
    static constexpr auto deEssId = "deEss";
    static constexpr auto compId = "comp";
    static constexpr auto gateId = "gate";
    static constexpr auto limiterId = "limiter";

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    AirBandDSP dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirBandAudioProcessor)
};
