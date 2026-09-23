#pragma once

#include "PluginProcessor.h"

class AirBandAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AirBandAudioProcessorEditor (AirBandAudioProcessor&);
    ~AirBandAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AirBandAudioProcessor& audioProcessor;

    juce::Slider midAirSlider, highAirSlider, blendSlider, outputSlider;
    juce::Label midAirLabel, highAirLabel, blendLabel, outputLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> midAirAttachment, highAirAttachment, blendAttachment, outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirBandAudioProcessorEditor)
};
