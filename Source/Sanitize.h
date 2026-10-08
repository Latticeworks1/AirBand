#pragma once

#include <algorithm>
#include <cmath>

#include "Realtime.h"

// Largest magnitude the plugin processes: 80 dB over full scale, 40 dB beyond the loudest input it is specified for.
inline constexpr float kMaxInputMagnitude = 1.0e4f;

// Replaces NaN and infinite samples with silence and limits the rest to kMaxInputMagnitude. Every stage keeps
// recursive state (filters, envelope followers, delay lines), so a single non-finite input sample would otherwise
// circulate there and mute the channel until the plugin is reloaded, and a finite sample near the largest float would
// overflow to infinity as soon as a stage multiplies it by a gain above one. The compressor reduces gain over a few
// milliseconds, not within the sample that calls for it, so it cannot be relied on to make room for such a sample.
inline void sanitizeInput (float* samples, int count) AIRBAND_NONBLOCKING
{
    for (int i = 0; i < count; ++i)
        samples[i] = std::isfinite (samples[i]) ? std::clamp (samples[i], -kMaxInputMagnitude, kMaxInputMagnitude) : 0.0f;
}
