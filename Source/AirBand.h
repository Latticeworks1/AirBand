#pragma once

#include "Realtime.h"
#include <juce_dsp/juce_dsp.h>
#include "EnvelopeDetector.h"
#include "LevelTracker.h"

// One companded high band: highpass split, dual-time-constant envelope
// detector, and a parallel main+side gain law modelled on the Dolby A
// encoder-only "air" mod (bands 3/4 run with no decode stage).
//
// The side path is a limiter whose contribution collapses toward zero as
// level rises, so quiet high-frequency detail gets boosted while loud
// transients pass close to unity gain. This is what makes the effect read
// as clarity rather than a static treble shelf.
//
// The level at which the boost has collapsed is placed relative to a
// reference: the band's own level as tracked over the last tens of seconds
// (LevelTracker), or the design reference while tracking is off. A recording
// tracked 12 dB hotter or quieter therefore meets the same law.
class AirBand
{
public:
    // designReferenceDb: the 90th percentile of the band's envelope, in dBFS, on the material the knee was designed
    // on; the tracker starts there and tracking off holds it.
    void prepare (double sampleRate, int maxBlockSize, float highpassHz, float designReferenceDb);
    void reset();

    // boostDb: maximum gain applied to the quietest content in this band.
    // kneeAboveReferenceDb: how far above the reference level the band's
    // envelope must be for the boost to have fully collapsed to unity (the
    // limiter threshold in the patent topology).
    // deEssAmount (0-1): how much of the boost is withdrawn where the
    // caller reports sibilance (see process()).
    void setParameters (float boostDb, float kneeAboveReferenceDb, float deEssAmount = 0.0f) AIRBAND_NONBLOCKING;

    void setLevelTracking (bool on) AIRBAND_NONBLOCKING;

    // The transport jumped: the envelope follows audio that is no longer playing and is cleared, and the level
    // tracker restarts unless the same material comes round again (a loop). The filters keep their state.
    void restartDynamics (bool restartTracking) AIRBAND_NONBLOCKING;
    void setTimeline (std::optional<std::int64_t> blockStart) AIRBAND_NONBLOCKING { tracker.setTimeline (blockStart); }

    // Processes one block, writing the band's "air" contribution (already
    // scaled by the boost/threshold law) into airOut. sibilance (0-1 per
    // sample, from SibilanceDetector) scales the boost down by deEssAmount
    // times its value, so the boost is withdrawn from sibilants and never
    // turns into a cut. The caller sums airOut back with the dry signal at
    // whatever blend ratio it wants.
    void process (const float* input, const float* sibilance, float* airOut, int numSamples) AIRBAND_NONBLOCKING;

    // How many samples airOut lags the input. The oversampler is linear phase, so this is a whole
    // number and the same at every frequency: the caller aligns the dry signal by delaying it by
    // exactly this much before summing, which keeps the sum free of comb filtering.
    int getLatencySamples() const;

private:
    float processSample (float x, float sibilance) AIRBAND_NONBLOCKING;
    void refreshThreshold() AIRBAND_NONBLOCKING;

    juce::dsp::IIR::Filter<float> highpass;
    juce::dsp::IIR::Coefficients<float>::Ptr highpassCoeffs;

    double sr = 44100.0;
    float highpassFreq = 3000.0f;

    // ~2 ms fast tracker, ~0.3 ms peak catch, ~150 ms program-dependent
    // release.
    EnvelopeDetector envelope;
    LevelTracker tracker;

    float boostLinearMax = 1.0f;
    float kneeAboveReferenceDb = 0.0f;
    float thresholdLinear = 1.0f;
    float deEssAmount = 0.0f;

    // Oversampled soft clip on the side path only, so a sibilant/transient
    // burst that beats the envelope follower folds back gently instead of
    // aliasing. 2x is enough to push the tanh's harmonics of boosted
    // high-band content safely past Nyquist without real CPU cost. The
    // FIR half-band filter is linear phase (a polyphase IIR one delays
    // frequencies unequally, so the side path could not be lined up with
    // the dry signal at every frequency).
    juce::dsp::Oversampling<float> oversampler { 1, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple };
};
