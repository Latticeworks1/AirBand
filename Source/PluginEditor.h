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

    juce::Slider midAirSlider, highAirSlider, blendSlider, outputSlider, deEssSlider, compSlider, gateSlider, limiterSlider;
    juce::Label midAirLabel, highAirLabel, blendLabel, outputLabel, deEssLabel, compLabel, gateLabel, limiterLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> midAirAttachment, highAirAttachment, blendAttachment, outputAttachment,
        deEssAttachment, compAttachment, gateAttachment, limiterAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirBandAudioProcessorEditor)
};
