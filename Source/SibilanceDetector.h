#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include "EnvelopeDetector.h"
#include "Realtime.h"

// Measures per sample how much of a signal's level lies in the sibilant region, as a value from 0 (none) to 1 (a
// fricative). The level of a fourth-order band-pass around 6 kHz is divided by the level of the whole signal, both
// followed by the same detector (1 ms attack, 50 ms release), and the ratio is mapped through a smooth ramp.
//
// Dividing by the whole signal makes the measure independent of level and of how much content the recording carries
// above the band, which a division by the air band's own level is not. On a 155 s vocal recording the ratio had a
// median of 0.025 and a 99th percentile of 0.128 over voiced frames and a median of 0.39 and a 10th percentile of 0.11
// over frames a spectrogram marks as sibilant (docs/TUNING.md); the ramp runs from 0.08 to 0.35. The band-pass is wide
// enough that a lone tone between 5 and 10 kHz reads as partly sibilant, and a flat noise floor reads as sibilant.
class SibilanceDetector
{
public:
    void prepare (double sampleRate)
    {
        const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, kCentreHz, kQ);
        first.coefficients = coefficients;
        second.coefficients = coefficients;
        bandLevel.prepare (sampleRate, kAttackSeconds, kAttackSeconds, kReleaseSeconds);
        totalLevel.prepare (sampleRate, kAttackSeconds, kAttackSeconds, kReleaseSeconds);
        reset();
    }

    void reset()
    {
        first.reset();
        second.reset();
        bandLevel.reset();
        totalLevel.reset();
    }

    float process (float x) AIRBAND_NONBLOCKING
    {
        AIRBAND_UNCHECKED_BEGIN
        const float band = second.processSample (first.processSample (x));
        AIRBAND_UNCHECKED_END

        const float ratio = bandLevel.pushSample (std::abs (band)) / juce::jmax (totalLevel.pushSample (std::abs (x)), 1.0e-6f);
        const float t = juce::jlimit (0.0f, 1.0f, (ratio - kRatioLow) / (kRatioHigh - kRatioLow));
        return t * t * (3.0f - 2.0f * t);
    }

    static constexpr float kCentreHz = 6000.0f;
    static constexpr float kQ = 1.5f;
    static constexpr float kAttackSeconds = 0.001f;
    static constexpr float kReleaseSeconds = 0.05f;
    static constexpr float kRatioLow = 0.08f;
    static constexpr float kRatioHigh = 0.35f;

private:
    juce::dsp::IIR::Filter<float> first, second;
    EnvelopeDetector bandLevel, totalLevel;
};
