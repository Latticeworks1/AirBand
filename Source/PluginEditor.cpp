#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    void setupRotary (juce::Slider& slider, juce::Label& label, const juce::String& text, juce::Component& parent)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
        parent.addAndMakeVisible (slider);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        parent.addAndMakeVisible (label);
    }
}

AirBandAudioProcessorEditor::AirBandAudioProcessorEditor (AirBandAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), audioProcessor (p)
{
    setupRotary (midAirSlider, midAirLabel, "Mid Air", *this);
    setupRotary (highAirSlider, highAirLabel, "High Air", *this);
    setupRotary (blendSlider, blendLabel, "Air Blend", *this);
    setupRotary (outputSlider, outputLabel, "Output", *this);
    setupRotary (deEssSlider, deEssLabel, "De-Ess", *this);

    midAirAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::midAirId, midAirSlider);
    highAirAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::highAirId, highAirSlider);
    blendAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::blendId, blendSlider);
    outputAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::outputId, outputSlider);
    deEssAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::deEssId, deEssSlider);

    setSize (500, 220);
}

void AirBandAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1e22));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawFittedText ("AirBand", getLocalBounds().removeFromTop (36), juce::Justification::centred, 1);
}

void AirBandAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (36);

    const int knobWidth = area.getWidth() / 5;

    auto layoutKnob = [&] (juce::Rectangle<int> bounds, juce::Slider& slider, juce::Label& label)
    {
        label.setBounds (bounds.removeFromTop (20));
        slider.setBounds (bounds);
    };

    layoutKnob (area.removeFromLeft (knobWidth), midAirSlider, midAirLabel);
    layoutKnob (area.removeFromLeft (knobWidth), highAirSlider, highAirLabel);
    layoutKnob (area.removeFromLeft (knobWidth), deEssSlider, deEssLabel);
    layoutKnob (area.removeFromLeft (knobWidth), blendSlider, blendLabel);
    layoutKnob (area.removeFromLeft (knobWidth), outputSlider, outputLabel);
}
