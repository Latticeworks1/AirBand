#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AirBandDSP.h"
#include "HostParameters.h"

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
    setupRotary (compSlider, compLabel, "Comp", *this);
    setupRotary (gateSlider, gateLabel, "Gate", *this);
    setupRotary (gateThresholdSlider, gateThresholdLabel, "Gate Thresh", *this);
    setupRotary (limiterSlider, limiterLabel, "Limiter", *this);

    midAirAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::midAirId, midAirSlider);
    highAirAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::highAirId, highAirSlider);
    blendAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::blendId, blendSlider);
    outputAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::outputId, outputSlider);
    deEssAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::deEssId, deEssSlider);
    compAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::compId, compSlider);
    gateAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::gateId, gateSlider);
    gateThresholdAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::gateThresholdId, gateThresholdSlider);
    limiterAttachment = std::make_unique<Attachment> (audioProcessor.apvts, AirBandAudioProcessor::limiterId, limiterSlider);

    addAndMakeVisible (levelTrackingButton);
    levelTrackingAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, AirBandAudioProcessor::levelTrackingId, levelTrackingButton);

    readoutLabel.setJustificationType (juce::Justification::centred);
    readoutLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (readoutLabel);
    updateReadout();

    setSize (855, 244);
    startTimerHz (10);
}

void AirBandAudioProcessorEditor::timerCallback()
{
    updateReadout();
}

// The Air knobs are the boost at 100 percent blend, so what a setting adds at low level is shown beside them.
void AirBandAudioProcessorEditor::updateReadout()
{
    const double rate = audioProcessor.getSampleRate() > 0.0 ? audioProcessor.getSampleRate() : 44100.0;
    const auto settings = host::readSettings (audioProcessor.apvts);
    const auto gain = [&] (double hz)
    {
        const float db = AirBandDSP::lowLevelGainDb (settings, hz, rate);
        return juce::String (db, 1);
    };

    const auto text = "Added at low level (the Air knobs are the boost at 100% blend):  5 kHz " + gain (5000.0) + " dB,  12 kHz " + gain (12000.0) + " dB";
    if (readoutLabel.getText() != text)
        readoutLabel.setText (text, juce::dontSendNotification);
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
    levelTrackingButton.setBounds (getWidth() - 16 - 110, 8, 110, 24);
    area.removeFromTop (36);
    readoutLabel.setBounds (area.removeFromBottom (24));

    const int knobWidth = area.getWidth() / 9;

    auto layoutKnob = [&] (juce::Rectangle<int> bounds, juce::Slider& slider, juce::Label& label)
    {
        label.setBounds (bounds.removeFromTop (20));
        slider.setBounds (bounds);
    };

    layoutKnob (area.removeFromLeft (knobWidth), gateSlider, gateLabel);
    layoutKnob (area.removeFromLeft (knobWidth), gateThresholdSlider, gateThresholdLabel);
    layoutKnob (area.removeFromLeft (knobWidth), compSlider, compLabel);
    layoutKnob (area.removeFromLeft (knobWidth), midAirSlider, midAirLabel);
    layoutKnob (area.removeFromLeft (knobWidth), highAirSlider, highAirLabel);
    layoutKnob (area.removeFromLeft (knobWidth), deEssSlider, deEssLabel);
    layoutKnob (area.removeFromLeft (knobWidth), blendSlider, blendLabel);
    layoutKnob (area.removeFromLeft (knobWidth), outputSlider, outputLabel);
    layoutKnob (area.removeFromLeft (knobWidth), limiterSlider, limiterLabel);
}
