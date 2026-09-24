#pragma once

#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "EnvelopeDetector.h"

// A broadband downward compressor sharing the same envelope-follower shape
// used by the air bands (see EnvelopeDetector.h), applied to the dry
// signal ahead of the air processing so the vocal is levelled before the
// air/de-ess stage adds high-frequency detail on top.
//
// Exposed as a single "amount" macro rather than separate threshold/ratio
// controls, matching the rest of this plugin's one-knob-per-effect layout.
// Threshold (-18 dBFS), the 1:1-4:1 ratio range, and the flat makeup-gain
// range (0-6 dB) are convention-carried defaults for a vocal bus, not
// swept or ablated against reference material.
class VocalCompressor
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    // amount: 0-1. At 0 the compressor is exactly transparent (ratio
    // collapses to 1:1 and makeup gain is 0 dB); increasing it raises both
    // the ratio and the makeup gain together.
    void setAmount (float amount);

    void process (juce::AudioBuffer<float>& buffer);

private:
    float processSample (float x, EnvelopeDetector& detector) const;

    std::vector<EnvelopeDetector> detectors;

    double sr = 44100.0;
    float thresholdDb = -18.0f;
    float ratio = 1.0f;
    float makeupLinear = 1.0f;
};
