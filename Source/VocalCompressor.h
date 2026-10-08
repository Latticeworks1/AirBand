#pragma once

#include "Realtime.h"
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "LevelTracker.h"

// A broadband downward compressor applied to the dry signal ahead of the air processing, so the
// vocal is levelled before the air/de-ess stage adds high-frequency detail on top.
//
// Exposed as a single "amount" macro rather than separate threshold/ratio controls, matching the
// rest of this plugin's one-knob-per-effect layout. The ratio runs from 1:1 to 4:1.
//
// Three things happen on three time scales, and each has its own state.
//
// Sample by sample the level is the instantaneous magnitude of the input in dB (floored at -120 dB),
// and the gain computer turns it into the reduction that level calls for: none below a 6 dB
// quadratic soft knee centred on the threshold, (1 - 1/ratio) times the excess above it.
//
// Over milliseconds that required reduction is smoothed, in dB. A peak hold releases toward the
// required reduction with a 100 ms time constant, so the reduction follows the peaks of the waveform
// at once and lets go slowly, and a one-pole smoother with a 5 ms time constant takes the held
// reduction to the gain that is applied. The attack and the release are therefore the same in dB
// at every level, and the gain does not ripple within a cycle of a low note; smoothing the
// required reduction directly with separate attack and release branches does, and distorts a 100 Hz
// tone 15 dB more (docs/TUNING.md).
//
// Over tens of seconds the threshold follows the material (LevelTracker). The tracker is fed the
// largest magnitude of each 10 ms, the quantity the gain computer sees, and its reference is the
// 90th percentile of those peaks. The threshold sits 3 dB below the reference, so the louder part of
// the material is compressed whatever level it was recorded at, and the makeup gain is the
// reduction the static curve applies at the reference level, so the reference passes at unity gain
// and the material below it is raised by the reduction applied to the material above. The makeup
// follows the static curve and the reference, never the smoothed reduction. While tracking is off the
// reference stays at the design value. The ratio range, knee width, the 3 dB offset and the time
// constants are convention-carried; the design reference is the 90th percentile of the 10 ms peaks
// of one 155 s vocal recording (docs/TUNING.md).
class VocalCompressor
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    // The transport jumped. The reduction and its peak hold belong to audio that is no longer playing and are
    // cleared; the level tracker restarts too unless the same material comes round again (a loop).
    void restartDynamics (bool restartTracking) AIRBAND_NONBLOCKING;
    void setTimeline (std::optional<std::int64_t> blockStart) AIRBAND_NONBLOCKING
    {
        for (auto& channel : channels)
            channel.tracker.setTimeline (blockStart);
    }

    // amount: 0-1. At 0 the compressor is exactly transparent (ratio
    // collapses to 1:1, so neither reduction nor makeup gain applies);
    // increasing it raises the ratio and, with it, the makeup gain.
    void setAmount (float amount) AIRBAND_NONBLOCKING;

    void setLevelTracking (bool on) AIRBAND_NONBLOCKING;

    void process (juce::AudioBuffer<float>& buffer) AIRBAND_NONBLOCKING;

    // Gain reduction in dB for a level: none below the knee, a quadratic transition across kKneeDb centred on
    // the threshold, and (1 - 1/ratio) times the excess above it.
    static float gainReductionDb (float levelDb, float thresholdDb, float ratio) AIRBAND_NONBLOCKING;

    static constexpr float kKneeDb = 6.0f;
    static constexpr float kThresholdBelowReferenceDb = 3.0f;
    static constexpr float kDesignReferenceDb = -14.3f;
    static constexpr float kAttackSeconds = 0.005f;
    static constexpr float kReleaseSeconds = 0.1f;
    static constexpr float kLevelFloorDb = -120.0f;

private:
    struct Channel
    {
        LevelTracker tracker;
        float blockPeak = 0.0f;   // largest magnitude since the tracker's last update
        float heldDb = 0.0f;      // peak-held required reduction
        float reductionDb = 0.0f; // applied reduction
        float thresholdDb = kDesignReferenceDb - kThresholdBelowReferenceDb;
        float makeupLinear = 1.0f;
    };

    void refresh (Channel& channel) const AIRBAND_NONBLOCKING;
    float processSample (float x, Channel& channel) const AIRBAND_NONBLOCKING;

    std::vector<Channel> channels;

    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    double sr = 44100.0;
    float ratio = 1.0f;
};
