#include "VocalGate.h"

namespace
{
    constexpr float kDetectorAttackSeconds = 0.001f;
    constexpr float kDetectorReleaseSeconds = 0.02f;
    constexpr float kSidechainHighpassHz = 100.0f;

    constexpr float kGainAttackSeconds = 0.002f;
    constexpr float kHoldSeconds = 0.04f;
    constexpr float kGainReleaseSeconds = 0.1f;
}

void VocalGate::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    detectors.assign ((size_t) numChannels, EnvelopeDetector());
    for (auto& detector : detectors)
        detector.prepare (sampleRate, kDetectorAttackSeconds, kDetectorAttackSeconds, kDetectorReleaseSeconds);

    const auto highpass = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, kSidechainHighpassHz, 0.707f);
    sidechainFilters.resize ((size_t) numChannels);
    for (auto& filter : sidechainFilters)
        filter.coefficients = highpass;

    scratch.assign ((size_t) maxBlockSize, 0.0f);

    gainAttackCoeff = std::exp (-1.0f / (kGainAttackSeconds * (float) sampleRate));
    gainReleaseCoeff = std::exp (-1.0f / (kGainReleaseSeconds * (float) sampleRate));
    holdSamples = (int) std::lround (kHoldSeconds * sampleRate);

    reset();
}

void VocalGate::reset()
{
    for (auto& detector : detectors)
        detector.reset();

    for (auto& filter : sidechainFilters)
        filter.reset();

    reductionDb = 0.0f;
    holdRemaining = 0;
}

void VocalGate::setAmount (float amount) AIRBAND_NONBLOCKING
{
    amount = juce::jlimit (0.0f, 1.0f, amount);
    ratio = 1.0f + amount * 3.0f;         // 1:1 .. 4:1 downward expansion
    maxAttenuationDb = amount * 18.0f;    // 0 .. 18 dB floor
}

void VocalGate::setThreshold (float newThresholdDb) AIRBAND_NONBLOCKING
{
    thresholdDb = newThresholdDb;
}

void VocalGate::restartDynamics() AIRBAND_NONBLOCKING
{
    for (auto& detector : detectors)
        detector.reset();

    reductionDb = 0.0f;
    holdRemaining = 0;
}

float VocalGate::nextGain (float detectedLevel) AIRBAND_NONBLOCKING
{
    const float levelDb = juce::Decibels::gainToDecibels (detectedLevel, -100.0f);

    float targetDb = 0.0f;
    if (levelDb < thresholdDb)
        targetDb = juce::jmin ((thresholdDb - levelDb) * (1.0f - 1.0f / ratio), maxAttenuationDb);
    else
        holdRemaining = holdSamples;

    if (targetDb > reductionDb)
    {
        // Closing: wait out the hold, then ease toward the target reduction.
        if (holdRemaining > 0)
            --holdRemaining;
        else
            reductionDb += (targetDb - reductionDb) * (1.0f - gainReleaseCoeff);
    }
    else
    {
        reductionDb += (targetDb - reductionDb) * (1.0f - gainAttackCoeff);

        // The smoothing approaches zero only asymptotically; settle it so a gate that is open (or off) is exactly unity.
        if (targetDb == 0.0f && reductionDb < 1.0e-6f)
            reductionDb = 0.0f;
    }

    return reductionDb > 0.0f ? juce::Decibels::decibelsToGain (-reductionDb) : 1.0f;
}

void VocalGate::process (juce::AudioBuffer<float>& buffer) AIRBAND_NONBLOCKING
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) detectors.size());
    const int numSamples = juce::jmin (buffer.getNumSamples(), (int) scratch.size());

    if (numChannels == 0 || numSamples == 0)
        return;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        auto& detector = detectors[(size_t) ch];
        auto& filter = sidechainFilters[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            AIRBAND_UNCHECKED_BEGIN
            const float keyed = filter.processSample (data[i]);
            AIRBAND_UNCHECKED_END
            const float level = detector.pushSample (std::abs (keyed));
            scratch[(size_t) i] = ch == 0 ? level : juce::jmax (scratch[(size_t) i], level);
        }
    }

    for (int i = 0; i < numSamples; ++i)
        scratch[(size_t) i] = nextGain (scratch[(size_t) i]);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] *= scratch[(size_t) i];
    }
}
