#pragma once

#include "AirBandSettings.h"

// Named parameter states shared by the specifications.
namespace tests
{
    // Every effect at its neutral value; only the always-present limiter lookahead remains. The air knees and the
    // compressor threshold are held at their design values, so a stage switched on from here follows its static law;
    // the tracking tests turn level tracking on.
    inline AirBandSettings transparentSettings()
    {
        return { .blend = 0.2f, .deEssAmount = 0.0f, .levelTracking = false };
    }

    inline AirBandSettings minimumSettings()
    {
        return { .blend = 0.0f, .limiterCeilingDb = -12.0f };
    }

    inline AirBandSettings maximumSettings()
    {
        return { .midAirDb = 15.0f, .highAirDb = 15.0f, .blend = 1.0f, .outputDb = 12.0f, .deEssAmount = 1.0f,
                 .compAmount = 1.0f, .gateAmount = 1.0f, .limiterCeilingDb = -12.0f };
    }

    // Every stage active at moderate settings.
    inline AirBandSettings featureSettings()
    {
        return { .midAirDb = 9.0f, .highAirDb = 9.0f, .blend = 0.4f, .outputDb = -2.0f, .deEssAmount = 0.5f,
                 .compAmount = 0.5f, .gateAmount = 0.5f, .limiterCeilingDb = -3.0f };
    }
}
