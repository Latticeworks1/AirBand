#pragma once

#include <cstdint>

namespace tests
{
    struct GoldenVector
    {
        const char* name;
        std::uint64_t hash; // bit pattern of every output sample (macOS arm64)
        double rmsDb;
        double peak;
    };

    inline constexpr GoldenVector kGoldenVectors[] = {
    { "impulse", 0xf56621937ec5f8b1ull, -46.513421, 0.517500043 },
    { "silence_to_transient", 0xdc4132cc7c3195f0ull, -30.277208, 0.49323681 },
    { "steady_sine", 0x4476125eddae7120ull, -19.650834, 0.240469843 },
    { "two_tone", 0x06db5a5f95f0fc84ull, -19.456236, 0.390614271 },
    { "noise", 0x119ae141a676e9c7ull, -19.949655, 0.358044654 },
    { "amplitude_steps", 0x5cff4e27d5d40f00ull, -21.017732, 0.465342134 },
    { "amplitude_steps_pinned", 0xaf6dfeb006de3979ull, -22.373978, 0.471968055 },
    { "parameter_steps", 0x8860763b4618fb02ull, -18.244653, 0.358044654 },
    { "block_edge_transients", 0xb6312bb322e2d0efull, -33.112237, 0.707945764 },
    { "sustained_10s", 0xf8a129b5b87af58bull, -16.279440, 0.315903306 },
    { "settings_minimum", 0x40febdc4588e03ecull, -16.817773, 0.251188636 },
    { "settings_default", 0x8b1ff7f46521690bull, -15.284611, 0.299996436 },
    { "settings_maximum", 0x4e5aa21d93701a52ull, -17.184158, 0.251188636 },
    };
}
