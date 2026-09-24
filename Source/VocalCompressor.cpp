#include "VocalCompressor.h"

void VocalCompressor::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;

    detectors.assign ((size_t) numChannels, EnvelopeDetector());
    for (auto& detector : detectors)
        detector.prepare (sr, 0.005f, 0.005f, 0.1f); // 5 ms attack, 100 ms release, no separate peak catch

    reset();
}

void VocalCompressor::reset()
{
    for (auto& detector : detectors)
        detector.reset();
}

void VocalCompressor::setAmount (float amount)
{
    amount = juce::jlimit (0.0f, 1.0f, amount);
    ratio = 1.0f + amount * 3.0f;         // 1:1 .. 4:1
    const float makeupDb = amount * 6.0f; // 0 .. +6 dB
    makeupLinear = juce::Decibels::decibelsToGain (makeupDb);
}

float VocalCompressor::processSample (float x, EnvelopeDetector& detector) const
{
    const float envelope = detector.pushSample (std::abs (x));
    const float levelDb = juce::Decibels::gainToDecibels (envelope, -100.0f);

    float gainReductionDb = 0.0f;
    if (levelDb > thresholdDb)
        gainReductionDb = (levelDb - thresholdDb) * (1.0f - 1.0f / ratio);

    const float gain = juce::Decibels::decibelsToGain (-gainReductionDb) * makeupLinear;
    return x * gain;
}

void VocalCompressor::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) detectors.size());

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& detector = detectors[(size_t) ch];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] = processSample (data[i], detector);
    }
}
