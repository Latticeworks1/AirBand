#include "VocalCompressor.h"

void VocalCompressor::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    attackCoeff = std::exp (-1.0f / (kAttackSeconds * (float) sr));
    releaseCoeff = std::exp (-1.0f / (kReleaseSeconds * (float) sr));

    channels.assign ((size_t) numChannels, Channel());
    for (auto& channel : channels)
        channel.tracker.prepare (sr, kDesignReferenceDb);

    reset();
}

void VocalCompressor::reset()
{
    for (auto& channel : channels)
    {
        channel.tracker.reset();
        channel.blockPeak = 0.0f;
        channel.heldDb = 0.0f;
        channel.reductionDb = 0.0f;
        refresh (channel);
    }
}

void VocalCompressor::restartDynamics (bool restartTracking) AIRBAND_NONBLOCKING
{
    for (auto& channel : channels)
    {
        if (restartTracking)
            channel.tracker.relocate();

        channel.blockPeak = 0.0f;
        channel.heldDb = 0.0f;
        channel.reductionDb = 0.0f;
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
    const float magnitude = std::abs (x);

    // The tracker takes the largest magnitude of its 10 ms interval; push() returns true on the update that used it.
    channel.blockPeak = juce::jmax (channel.blockPeak, magnitude);
    if (channel.tracker.push (channel.blockPeak))
    {
        channel.blockPeak = 0.0f;
        refresh (channel);
    }

    const float required = gainReductionDb (juce::Decibels::gainToDecibels (magnitude, kLevelFloorDb), channel.thresholdDb, ratio);

    channel.heldDb = juce::jmax (required, releaseCoeff * channel.heldDb + (1.0f - releaseCoeff) * required);
    channel.reductionDb = attackCoeff * channel.reductionDb + (1.0f - attackCoeff) * channel.heldDb;

    // The smoothing approaches zero only asymptotically; settle it so a compressor that is idle (or off) is exactly unity.
    if (channel.heldDb < 1.0e-6f)
    {
        channel.heldDb = 0.0f;
        if (channel.reductionDb < 1.0e-6f)
            channel.reductionDb = 0.0f;
    }

    return x * juce::Decibels::decibelsToGain (-channel.reductionDb) * channel.makeupLinear;
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
