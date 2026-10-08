#pragma once

#include "PluginProcessor.h"

class AirBandAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AirBandAudioProcessorEditor (AirBandAudioProcessor&);
    ~AirBandAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateReadout();

    AirBandAudioProcessor& audioProcessor;

    juce::Slider midAirSlider, highAirSlider, blendSlider, outputSlider, deEssSlider, compSlider, gateSlider, gateThresholdSlider, limiterSlider;
    juce::Label midAirLabel, highAirLabel, blendLabel, outputLabel, deEssLabel, compLabel, gateLabel, gateThresholdLabel, limiterLabel;

    juce::ToggleButton levelTrackingButton { "Auto Level" };
    juce::Label readoutLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<Attachment> midAirAttachment, highAirAttachment, blendAttachment, outputAttachment,
        deEssAttachment, compAttachment, gateAttachment, gateThresholdAttachment, limiterAttachment;
    std::unique_ptr<ButtonAttachment> levelTrackingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirBandAudioProcessorEditor)
};
