#include "VocalCompressor.h"

void VocalCompressor::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;

    channels.assign ((size_t) numChannels, Channel());
    for (auto& channel : channels)
    {
        channel.detector.prepare (sr, 0.005f, 0.005f, 0.1f); // 5 ms attack, 100 ms release, no separate peak catch
        channel.tracker.prepare (sr, kDesignReferenceDb);
    }

    reset();
}

void VocalCompressor::reset()
{
    for (auto& channel : channels)
    {
        channel.detector.reset();
        channel.tracker.reset();
        refresh (channel);
    }
}

float VocalCompressor::gainReductionDb (float levelDb, float thresholdDb, float ratioIn) AIRBAND_NONBLOCKING
{
    const float over = levelDb - thresholdDb;
    const float slope = 1.0f - 1.0f / ratioIn;

    if (2.0f * over < -kKneeDb)
        return 0.0f;

    if (2.0f * over <= kKneeDb)
    {
        const float t = over + 0.5f * kKneeDb;
        return slope * t * t / (2.0f * kKneeDb);
    }

    return slope * over;
}

void VocalCompressor::refresh (Channel& channel) const AIRBAND_NONBLOCKING
{
    const float reference = channel.tracker.referenceDb();
    channel.thresholdDb = reference - kThresholdBelowReferenceDb;
    channel.makeupLinear = juce::Decibels::decibelsToGain (gainReductionDb (reference, channel.thresholdDb, ratio));
}

void VocalCompressor::setAmount (float amount) AIRBAND_NONBLOCKING
{
    amount = juce::jlimit (0.0f, 1.0f, amount);
    ratio = 1.0f + amount * 3.0f; // 1:1 .. 4:1

    for (auto& channel : channels)
        refresh (channel);
}

void VocalCompressor::setLevelTracking (bool on) AIRBAND_NONBLOCKING
{
    for (auto& channel : channels)
        channel.tracker.setTracking (on);
}

float VocalCompressor::processSample (float x, Channel& channel) const AIRBAND_NONBLOCKING
{
    const float envelope = channel.detector.pushSample (std::abs (x));
    if (channel.tracker.push (envelope))
        refresh (channel);

    const float levelDb = juce::Decibels::gainToDecibels (envelope, -100.0f);
    const float gainReduction = gainReductionDb (levelDb, channel.thresholdDb, ratio);

    return x * juce::Decibels::decibelsToGain (-gainReduction) * channel.makeupLinear;
}

void VocalCompressor::process (juce::AudioBuffer<float>& buffer) AIRBAND_NONBLOCKING
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) channels.size());

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& channel = channels[(size_t) ch];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] = processSample (data[i], channel);
    }
}
