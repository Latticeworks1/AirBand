#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Realtime.h"

// What the host did to the timeline between two blocks that were not consecutive.
enum class TransportEvent
{
    loopWrap, // a backward jump while the host is looping: the same material comes round again
    jump      // any other jump (locate, scrub, start from a new position): the material may be a different part of the song
};

// Decides from the host's transport position whether a block continues the previous one.
//
// The playback position of a block should equal the position of the previous block plus its length. A block that
// starts elsewhere is the start of audio that has no causal relation to the signal the level trackers, envelope
// followers and gain smoothers have been following, so the plugin restarts them (AirBandDSP::noteTransportEvent).
//
// Only blocks the host reports as playing or recording count. While the transport is stopped the audio is live input
// or silence and the position does not describe it, so those blocks neither raise an event nor move the expected
// position; pausing and resuming at the same place is continuous, and resuming somewhere else is a jump. A position
// that is off by up to a block length, or by 20 ms, is taken as continuous because hosts that split blocks at
// automation points may report the start of the whole buffer for every part of it.
class TransportMonitor
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        tolerance = std::max<std::int64_t> (maxBlockSize, (std::int64_t) std::llround (kToleranceSeconds * sampleRate));
        sr = sampleRate;
        reset();
    }

    void reset() { haveExpected = false; }

    std::optional<TransportEvent> observe (bool playing, std::optional<std::int64_t> position, bool looping, int numSamples) AIRBAND_NONBLOCKING
    {
        start.reset();
        if (! playing || ! position)
            return std::nullopt;

        start = position;
        std::optional<TransportEvent> event;
        if (haveExpected)
        {
            const auto error = *position - expected;
            if (error > tolerance || error < -tolerance)
                event = looping && error < 0 ? TransportEvent::loopWrap : TransportEvent::jump;
        }

        expected = *position + numSamples;
        haveExpected = true;
        return event;
    }

    // The position in samples if the host gives one, else its time in seconds converted.
    std::optional<TransportEvent> observe (const juce::AudioPlayHead::PositionInfo& info, int numSamples) AIRBAND_NONBLOCKING
    {
        std::optional<std::int64_t> position;
        if (const auto samples = info.getTimeInSamples())
            position = (std::int64_t) *samples;
        else if (const auto seconds = info.getTimeInSeconds())
            position = (std::int64_t) std::llround (*seconds * sr);

        return observe (info.getIsPlaying() || info.getIsRecording(), position, info.getIsLooping(), numSamples);
    }

    // The position of the first sample of the block last observed, if it was a playing block with a position.
    std::optional<std::int64_t> blockStart() const { return start; }

    static constexpr double kToleranceSeconds = 0.02;

private:
    double sr = 44100.0;
    std::int64_t tolerance = 882;
    std::int64_t expected = 0;
    std::optional<std::int64_t> start;
    bool haveExpected = false;
};
