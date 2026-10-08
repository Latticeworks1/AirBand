#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <juce_core/juce_core.h>
#include "Realtime.h"

// Follows a high percentile of a level signal, in dB, so that a threshold can be placed relative to
// the level the material actually has instead of at a level fixed when the plugin was designed.
//
// Every 10 ms the tracker adds the current level to a histogram of 1 dB bins whose contents fade
// exponentially, so the histogram describes the last few tens of seconds of activity, and the
// reference is the level that a fraction (1 - percentile) of that recent activity exceeds. Fading
// only advances while there is activity, so a pause leaves the reference where it was. Two gates
// decide what is activity: a level under a floor (digital silence) never counts, and once the
// reference has been acquired a level more than kGateBelowDb under it does not count either, so
// room noise between phrases, a long pause or a sparse passage cannot pull the percentile down.
// The estimate cannot leave a window around the design level.
//
// There is no history at the start and none that describes the signal after the transport jumps to
// another part of the material (restart()). The reference then holds its current value, which is
// the design level at the start, while the first kAcquireUpdates active levels arrive (300 ms) and
// moves to the percentile of what has arrived in proportion to how much of that there is, so a
// start or a jump does not step the thresholds, and the relative gate is off during this time so
// that material far under the old reference is heard. After that the histogram fills at the usual
// rate and the 20 s memory takes over.
//
// When the host reports where on the timeline each block lies (setTimeline()), the tracker also keeps a snapshot
// of its histogram every 5 s of playing audio, up to 24 of them, one per region of the timeline (a region that
// is played again replaces its snapshot). A jump to a position within 10 s of a snapshot (relocate()) takes the
// snapshot up as the history, since the material there is the material the snapshot was taken from, and only a
// jump to a region that has not been played restarts.
//
// referenceDb() is the estimate smoothed over about 100 ms, or the design level itself while
// tracking is off, so switching tracking moves thresholds as a short glide and not as a step.
class LevelTracker
{
public:
    // The memory, the gate and the acquisition length can be changed for a measurement (docs/tuning/TransportProbe.cpp);
    // the plugin uses the defaults.
    struct Options
    {
        float memorySeconds = kMemorySeconds;
        float gateBelowDb = kGateBelowDb;   // 0 turns the relative gate off
        int acquireUpdates = kAcquireUpdates;
    };

    void prepare (double sampleRate, float designDb, float memorySeconds = kMemorySeconds)
    {
        prepare (sampleRate, designDb, Options { memorySeconds });
    }

    void prepare (double sampleRate, float designDb, const Options& options)
    {
        design = designDb;
        gateBelowDb = options.gateBelowDb;
        acquireUpdates = juce::jmax (1, options.acquireUpdates);
        const float memorySeconds = options.memorySeconds;
        updateInterval = juce::jmax (1, (int) std::lround (kUpdateSeconds * sampleRate));
        spacing = (std::int64_t) kSaveUpdates * updateInterval;
        fade = std::exp (-(float) kUpdateSeconds / memorySeconds);
        reset();
    }

    // The signal starts: no history, the reference at the design level.
    void reset()
    {
        estimate = design;
        smoothed = design;
        counter = 0;
        sinceSave = 0;
        saves = 0;
        for (auto& slot : slots)
            slot.valid = false;

        restart();
    }

    // The host's position, in samples, of the first sample of the block about to be processed; nothing if it does not
    // report one. The tracker counts samples from there.
    void setTimeline (std::optional<std::int64_t> blockStart) AIRBAND_NONBLOCKING
    {
        clocked = blockStart.has_value();
        if (clocked)
            clock = *blockStart;
    }

    // The transport jumped to the position given to setTimeline(): the tracker takes up the snapshot of the
    // nearest region that was played, if there is one within 10 s, and else restarts.
    void relocate() AIRBAND_NONBLOCKING
    {
        const Snapshot* nearest = nullptr;
        if (clocked)
            for (const auto& slot : slots)
                if (slot.valid && std::llabs (slot.key - clock) <= 2 * spacing && (nearest == nullptr || std::llabs (slot.key - clock) < std::llabs (nearest->key - clock)))
                    nearest = &slot;

        if (nearest == nullptr)
        {
            restart();
            return;
        }

        bins = nearest->bins;
        mass = nearest->mass;
        estimate = nearest->estimate;
        acquired = acquireUpdates;
        smoothed = estimate;    // the reference is known, and the audio is discontinuous at the jump anyway
        hold = smoothed;
    }

    // The history no longer describes the signal. The reference keeps its current value until new levels arrive.
    void restart() AIRBAND_NONBLOCKING
    {
        bins.fill (0.0f);
        mass = 0.0f;
        acquired = 0;
        hold = smoothed;
        estimate = smoothed;
    }

    // The history cannot cover a time in which tracking was off.
    void setTracking (bool on) AIRBAND_NONBLOCKING
    {
        if (on && ! tracking)
            restart();

        tracking = on;
    }

    // level: the linear level (an envelope or a peak) at this sample. Returns true when referenceDb() has been
    // updated, once every 10 ms; the level then pushed is the one that counts.
    bool push (float level) AIRBAND_NONBLOCKING
    {
        clock += clocked ? 1 : 0;

        if (++counter < updateInterval)
            return false;

        counter = 0;

        if (tracking)
        {
            const float levelDb = 20.0f * std::log10 (juce::jmax (level, 1.0e-9f));
            const bool acquiring = acquired < acquireUpdates;
            if (levelDb > kFloorDb && (acquiring || gateBelowDb <= 0.0f || levelDb >= estimate - gateBelowDb))
            {
                for (auto& bin : bins)
                    bin = bin * fade < 1.0e-6f ? 0.0f : bin * fade;

                mass *= fade;
                bins[(size_t) juce::jlimit (0, kBins - 1, (int) std::floor (levelDb - kLowestDb))] += 1.0f;
                mass += 1.0f;
                if (acquiring)
                    ++acquired;

                const float weight = juce::jmin (1.0f, (float) acquired / (float) acquireUpdates);
                estimate = juce::jlimit (design - kWindowDb, design + kWindowDb, weight * histogramPercentile() + (1.0f - weight) * hold);
            }

            if (clocked && acquired >= acquireUpdates && ++sinceSave >= kSaveUpdates)
            {
                sinceSave = 0;
                save();
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
    static constexpr float kGateBelowDb = 30.0f;
    static constexpr int kAcquireUpdates = 30;
    static constexpr int kSaveUpdates = 500;        // 5 s
    static constexpr int kSlots = 24;               // 2 minutes of timeline

private:
    static constexpr double kUpdateSeconds = 0.01;
    static constexpr float kSmoothing = 0.1f;        // per update: about 100 ms
    static constexpr float kLowestDb = -100.0f;
    static constexpr int kBins = 101;

    // The level that a fraction (1 - percentile) of the faded observations exceed, taken as uniform within a bin.
    float histogramPercentile() const AIRBAND_NONBLOCKING
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

    struct Snapshot
    {
        std::array<float, kBins> bins {};
        float mass = 0.0f;
        float estimate = 0.0f;
        std::int64_t key = 0;
        std::uint32_t stamp = 0;
        bool valid = false;
    };

    // The region of the timeline at the clock takes the place of a snapshot less than half a spacing away, else of the oldest.
    void save() AIRBAND_NONBLOCKING
    {
        Snapshot* target = nullptr;
        for (auto& slot : slots)
            if (slot.valid && std::llabs (slot.key - clock) * 2 < spacing)
                target = &slot;

        if (target == nullptr)
        {
            target = &slots[0];
            for (auto& slot : slots)
                if (! slot.valid || (target->valid && slot.stamp < target->stamp))
                    target = &slot;
        }

        target->bins = bins;
        target->mass = mass;
        target->estimate = estimate;
        target->key = clock;
        target->stamp = ++saves;
        target->valid = true;
    }

    std::array<Snapshot, kSlots> slots {};
    std::int64_t clock = 0;
    std::int64_t spacing = 44100 * 5;
    std::uint32_t saves = 0;
    int sinceSave = 0;
    bool clocked = false;

    std::array<float, kBins> bins {};
    float design = 0.0f;
    float estimate = 0.0f;
    float smoothed = 0.0f;
    float hold = 0.0f;
    float mass = 0.0f;
    float fade = 0.999f;
    float gateBelowDb = kGateBelowDb;
    int acquireUpdates = kAcquireUpdates;
    bool tracking = true;
    int updateInterval = 441;
    int counter = 0;
    int acquired = 0;
};
