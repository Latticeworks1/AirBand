#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>

// Dual time-constant envelope follower: a fast integrator for general level
// tracking, and a separate (optionally faster) peak catch so a transient
// can't punch through before the slower detector reacts. Passing the same
// attack time for both collapses this to a plain single-attack follower,
// which is how the de-esser sub-band and the compressor below use it.
//
// This is the one detector shape used everywhere gain is derived from
// signal level in this plugin (the air bands, the sibilance sub-band, and
// the compressor), so a change to how level is tracked only has to be made
// once and applies consistently everywhere.
class EnvelopeDetector
{
public:
    void prepare (double sampleRate, float attackFastSeconds, float attackPeakSeconds, float releaseSeconds)
    {
        attackCoeffFast = std::exp (-1.0f / (attackFastSeconds * (float) sampleRate));
        attackCoeffPeak = std::exp (-1.0f / (attackPeakSeconds * (float) sampleRate));
        releaseCoeff = std::exp (-1.0f / (releaseSeconds * (float) sampleRate));
        reset();
    }

    void reset()
    {
        envelopeFast = 0.0f;
        envelopePeak = 0.0f;
    }

    // rectifiedInput: std::abs() of the signal to track. Returns the
    // current envelope value (the max of the two trackers).
    float pushSample (float rectifiedInput)
    {
        if (rectifiedInput > envelopeFast)
            envelopeFast = attackCoeffFast * envelopeFast + (1.0f - attackCoeffFast) * rectifiedInput;
        else
            envelopeFast = releaseCoeff * envelopeFast + (1.0f - releaseCoeff) * rectifiedInput;

        if (rectifiedInput > envelopePeak)
            envelopePeak = attackCoeffPeak * envelopePeak + (1.0f - attackCoeffPeak) * rectifiedInput;
        else
            envelopePeak = releaseCoeff * envelopePeak + (1.0f - releaseCoeff) * rectifiedInput;

        return juce::jmax (envelopeFast, envelopePeak);
    }

private:
    float envelopeFast = 0.0f;
    float envelopePeak = 0.0f;
    float attackCoeffFast = 0.0f;
    float attackCoeffPeak = 0.0f;
    float releaseCoeff = 0.0f;
};
