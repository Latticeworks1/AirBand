#pragma once

#include "Realtime.h"
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "EnvelopeDetector.h"
#include "LevelTracker.h"

// A broadband downward compressor sharing the same envelope-follower shape
// used by the air bands (see EnvelopeDetector.h), applied to the dry
// signal ahead of the air processing so the vocal is levelled before the
// air/de-ess stage adds high-frequency detail on top.
//
// Exposed as a single "amount" macro rather than separate threshold/ratio
// controls, matching the rest of this plugin's one-knob-per-effect layout.
// The ratio runs from 1:1 to 4:1 and the gain computer has a 6 dB quadratic
// soft knee. Threshold and makeup are placed relative to a reference level,
// the 90th percentile of the follower's level as tracked over the last tens
// of seconds (LevelTracker): the threshold sits 3 dB below it, so the louder
// part of the material is compressed whatever level it was recorded at, and
// the makeup gain is the reduction applied at the reference level, so the
// reference passes at unity gain and the material below it is raised by the
// reduction applied to the material above. While tracking is off the
// reference stays at the design value. The ratio range, knee width and the 3
// dB offset are convention-carried; the design reference is the 90th
// percentile on one 155 s vocal recording (docs/TUNING.md).
class VocalCompressor
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    // amount: 0-1. At 0 the compressor is exactly transparent (ratio
    // collapses to 1:1, so neither reduction nor makeup gain applies);
    // increasing it raises the ratio and, with it, the makeup gain.
    void setAmount (float amount) AIRBAND_NONBLOCKING;

    void setLevelTracking (bool on) AIRBAND_NONBLOCKING;

    void process (juce::AudioBuffer<float>& buffer) AIRBAND_NONBLOCKING;

    // Gain reduction in dB for a detected level: none below the knee, a quadratic transition across kKneeDb centred on
    // the threshold, and (1 - 1/ratio) times the excess above it.
    static float gainReductionDb (float levelDb, float thresholdDb, float ratio) AIRBAND_NONBLOCKING;

    static constexpr float kKneeDb = 6.0f;
    static constexpr float kThresholdBelowReferenceDb = 3.0f;
    static constexpr float kDesignReferenceDb = -18.6f;

private:
    struct Channel
    {
        EnvelopeDetector detector;
        LevelTracker tracker;
        float thresholdDb = kDesignReferenceDb - kThresholdBelowReferenceDb;
        float makeupLinear = 1.0f;
    };

    void refresh (Channel& channel) const AIRBAND_NONBLOCKING;
    float processSample (float x, Channel& channel) const AIRBAND_NONBLOCKING;

    std::vector<Channel> channels;

    double sr = 44100.0;
    float ratio = 1.0f;
};
