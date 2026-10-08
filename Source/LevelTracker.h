#pragma once

#include <array>
#include <cmath>
#include <juce_core/juce_core.h>
#include "Realtime.h"

// Follows a high percentile of a level signal, in dB, so that a threshold can be placed relative to
// the level the material actually has instead of at a level fixed when the plugin was designed.
//
// Every 10 ms the tracker adds the current level to a histogram of 1 dB bins whose contents fade
// exponentially, so the histogram describes the last few tens of seconds of activity, and the
// reference is the level that a fraction (1 - percentile) of that recent activity exceeds. Levels
// under a floor (silence between phrases) are not counted, and fading only advances while there
// is activity, so a pause leaves the reference where it was. A weighted pseudo-observation at the
// design level stands in for the history that does not exist yet and fades like any other
// observation, and the estimate cannot leave a window around the design level.
//
// referenceDb() is the estimate smoothed over about 100 ms, or the design level itself while
// tracking is off, so switching tracking moves thresholds as a short glide and not as a step.
class LevelTracker
{
public:
    void prepare (double sampleRate, float designDb, float memorySeconds = kMemorySeconds)
    {
        design = designDb;
        updateInterval = juce::jmax (1, (int) std::lround (kUpdateSeconds * sampleRate));
        fade = std::exp (-(float) kUpdateSeconds / memorySeconds);
        reset();
    }

    void reset()
    {
        bins.fill (0.0f);
        mass = 0.0f;
        priorMass = kPriorUpdates;
        estimate = design;
        smoothed = design;
        counter = 0;
    }

    void setTracking (bool on) AIRBAND_NONBLOCKING { tracking = on; }

    // level: the linear level (an envelope) at this sample. Returns true when referenceDb() has changed.
    bool push (float level) AIRBAND_NONBLOCKING
    {
        if (++counter < updateInterval)
            return false;

        counter = 0;

        if (tracking)
        {
            const float levelDb = 20.0f * std::log10 (juce::jmax (level, 1.0e-9f));
            if (levelDb > kFloorDb)
            {
                for (auto& bin : bins)
                    bin = bin * fade < 1.0e-6f ? 0.0f : bin * fade;

                mass *= fade;
                priorMass *= fade;
                bins[(size_t) juce::jlimit (0, kBins - 1, (int) std::floor (levelDb - kLowestDb))] += 1.0f;
                mass += 1.0f;
                estimate = juce::jlimit (design - kWindowDb, design + kWindowDb, blendedEstimate());
            }
        }

        smoothed += kSmoothing * ((tracking ? estimate : design) - smoothed);
        return true;
    }

    float referenceDb() const { return smoothed; }

    static constexpr float kPercentile = 0.9f;
    static constexpr float kMemorySeconds = 20.0f;
    static constexpr float kFloorDb = -90.0f;
    static constexpr float kWindowDb = 18.0f;

private:
    static constexpr double kUpdateSeconds = 0.01;
    static constexpr float kSmoothing = 0.1f;        // per update: about 100 ms
    static constexpr float kPriorUpdates = 100.0f;   // one second of observations at the design level
    static constexpr float kLowestDb = -100.0f;
    static constexpr int kBins = 101;

    // The level that a fraction (1 - percentile) of the faded observations exceed, taken as uniform within a bin.
    float histogramPercentile() const
    {
        const float target = (1.0f - kPercentile) * mass;
        float above = 0.0f;
        for (int b = kBins - 1; b >= 0; --b)
        {
            const float count = bins[(size_t) b];
            if (above + count >= target && count > 0.0f)
                return kLowestDb + (float) b + 1.0f - (target - above) / count;
            above += count;
        }
        return kLowestDb;
    }

    float blendedEstimate() const
    {
        return (histogramPercentile() * mass + design * priorMass) / (mass + priorMass);
    }

    std::array<float, kBins> bins {};
    float design = 0.0f;
    float estimate = 0.0f;
    float smoothed = 0.0f;
    float mass = 0.0f;
    float priorMass = 0.0f;
    float fade = 0.999f;
    bool tracking = true;
    int updateInterval = 441;
    int counter = 0;
};
