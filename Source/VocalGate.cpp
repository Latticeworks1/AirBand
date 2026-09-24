#include "VocalGate.h"

void VocalGate::prepare (double sampleRate, int numChannels)
{
    detectors.assign ((size_t) numChannels, EnvelopeDetector());
    for (auto& detector : detectors)
        detector.prepare (sampleRate, 0.003f, 0.003f, 0.2f); // 3 ms attack, 200 ms release

    reset();
}

void VocalGate::reset()
{
    for (auto& detector : detectors)
        detector.reset();
}

void VocalGate::setAmount (float amount)
{
    amount = juce::jlimit (0.0f, 1.0f, amount);
    ratio = 1.0f + amount * 3.0f;         // 1:1 .. 4:1 downward expansion
    maxAttenuationDb = amount * 18.0f;    // 0 .. 18 dB floor
}

float VocalGate::processSample (float x, EnvelopeDetector& detector) const
{
    const float envelope = detector.pushSample (std::abs (x));
    const float levelDb = juce::Decibels::gainToDecibels (envelope, -100.0f);

    float attenuationDb = 0.0f;
    if (levelDb < thresholdDb)
        attenuationDb = juce::jmin ((thresholdDb - levelDb) * (1.0f - 1.0f / ratio), maxAttenuationDb);

    return x * juce::Decibels::decibelsToGain (-attenuationDb);
}

void VocalGate::process (juce::AudioBuffer<float>& buffer)
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
