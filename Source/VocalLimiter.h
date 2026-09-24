#pragma once

#include <vector>
#include <juce_dsp/juce_dsp.h>

// A stereo-linked lookahead peak limiter, the final stage in the chain,
// catching whatever the gate/compressor/air/blend/output stages upstream
// stack up to. Peak (not RMS) detection is scanned across a fixed
// lookahead window ahead of the delayed output sample, so gain reduction
// starts ramping down before a transient arrives rather than reacting
// after the fact (which would either clip or need much harsher release).
//
// This introduces real latency (the lookahead delay), which the caller
// must report to the host via AudioProcessor::setLatencySamples() --
// silently delaying the signal without reporting it would desync AirBand
// against unprocessed tracks in the host. 5 ms lookahead and 60 ms
// release are convention-carried defaults for a safety limiter, not
// swept/ablated.
class VocalLimiter
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setCeilingDb (float ceilingDb);

    int getLatencySamples() const { return lookaheadSamples; }

    void process (juce::AudioBuffer<float>& buffer);

private:
    double sr = 44100.0;
    int lookaheadSamples = 0;
    int ringSize = 0;
    int writeIndex = 0;

    float ceilingLinear = 1.0f;
    float releaseCoeff = 0.0f;
    float currentGain = 1.0f;

    std::vector<std::vector<float>> delayLines; // [channel][ringIndex]
    std::vector<float> peakRing;                // max |sample| across channels, per ring slot
};
