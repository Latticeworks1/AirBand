#pragma once

#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "EnvelopeDetector.h"

// A broadband downward expander/gate applied to the dry signal before the
// compressor, so quiet breath and mouth noise gets attenuated ahead of the
// compressor's makeup gain rather than boosted along with it.
//
// Shares EnvelopeDetector with the air bands and VocalCompressor; a 3 ms
// attack lets a returning word through cleanly, a 200 ms release avoids
// the gate chattering on a vocal's natural decay tail. Threshold (-40
// dBFS), the 1:1-4:1 expansion ratio, and the -18 dB attenuation floor are
// convention-carried defaults for a vocal track, not swept/ablated.
class VocalGate
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    // amount: 0-1. At 0 the gate is exactly transparent (ratio collapses
    // to 1:1, so attenuation is always 0 regardless of level).
    void setAmount (float amount);

    void process (juce::AudioBuffer<float>& buffer);

private:
    float processSample (float x, EnvelopeDetector& detector) const;

    std::vector<EnvelopeDetector> detectors;

    float thresholdDb = -40.0f;
    float ratio = 1.0f;
    float maxAttenuationDb = 0.0f;
};
