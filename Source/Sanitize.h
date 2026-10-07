#pragma once

#include <cmath>

#include "Realtime.h"

// Replaces NaN and infinite samples with silence. Every stage keeps recursive state (filters,
// envelope followers, delay lines), so a single non-finite input sample would otherwise
// circulate there and mute the channel until the plugin is reloaded.
inline void zeroNonFinite (float* samples, int count) AIRBAND_NONBLOCKING
{
    for (int i = 0; i < count; ++i)
        if (! std::isfinite (samples[i]))
            samples[i] = 0.0f;
}
