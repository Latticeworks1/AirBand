#pragma once

// The complete parameter state of the DSP, in DSP units (dB, or 0-1
// fractions). Defaults are the product defaults; the plugin builds its host
// parameter defaults from the same values.
struct AirBandSettings
{
    float midAirDb = 0.0f;
    float highAirDb = 0.0f;
    float blend = 0.2f;
    float outputDb = 0.0f;
    float deEssAmount = 0.5f;
    float compAmount = 0.0f;
    float gateAmount = 0.0f;
    float limiterCeilingDb = 0.0f;
    float gateThresholdDb = -40.0f; // dBFS; the level below which the gate expands

    // The air knees and the compressor threshold follow the level of the material (see LevelTracker.h); off holds
    // them at their design values.
    bool levelTracking = true;

    bool operator== (const AirBandSettings&) const = default;
};
