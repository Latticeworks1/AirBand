#pragma once

#include "Realtime.h"
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "EnvelopeDetector.h"

// A broadband downward expander/gate applied to the dry signal before the
// compressor, so quiet breath and mouth noise gets attenuated ahead of the
// compressor's makeup gain rather than boosted along with it.
//
// Level detection and gain smoothing are separate stages. The detector
// follows the signal closely (1 ms attack, 20 ms release) after a 100 Hz
// high-pass that keeps rumble, handling noise and plosive thump from
// holding the gate open; the filter touches only the detection path, never
// the audio. A static expander law then maps the detected level to a gain
// reduction in dB, and that reduction is smoothed with its own timing:
// 2 ms to open, a 40 ms hold after the level falls below the threshold, and
// a 100 ms release toward the full reduction. All channels share one
// detection level (the loudest) and one gain, so a stereo image cannot
// shift when the channels differ in level.
//
// The threshold is a parameter. The remaining constants (high-pass corner,
// ballistics, 1:1-4:1 ratio, 18 dB attenuation floor) were chosen inside the
// ranges quoted for vocal gates (attack 1-3 ms, hold 20-100 ms, release
// 50-120 ms) and are not swept/ablated.
class VocalGate
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // amount: 0-1. At 0 the gate is exactly transparent (ratio collapses
    // to 1:1, so attenuation is always 0 regardless of level).
    void setAmount (float amount) AIRBAND_NONBLOCKING;

    // Detected level, in dBFS, below which the signal is expanded.
    void setThreshold (float thresholdDb) AIRBAND_NONBLOCKING;

    // The transport jumped: the detector level, the hold and the reduction belong to audio that is no longer playing,
    // so the gate opens. The sidechain filters keep their state.
    void restartDynamics() AIRBAND_NONBLOCKING;

    void process (juce::AudioBuffer<float>& buffer) AIRBAND_NONBLOCKING;

private:
    float nextGain (float detectedLevel) AIRBAND_NONBLOCKING;

    std::vector<EnvelopeDetector> detectors;
    std::vector<juce::dsp::IIR::Filter<float>> sidechainFilters;

    // Holds the linked detection level per sample, then the gain per sample.
    std::vector<float> scratch;

    float thresholdDb = -40.0f;
    float ratio = 1.0f;
    float maxAttenuationDb = 0.0f;

    float gainAttackCoeff = 0.0f;
    float gainReleaseCoeff = 0.0f;
    int holdSamples = 0;

    float reductionDb = 0.0f;
    int holdRemaining = 0;
};
