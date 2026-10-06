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
    { "impulse", 0x3d8b4c6c225b8b1bull, -48.318130, 0.296793789 },
    { "silence_to_transient", 0xa7df355e0dd6df41ull, -28.872784, 0.707945764 },
    { "steady_sine", 0x3ad4e6c7c5b1bc33ull, -17.509124, 0.300900161 },
    { "two_tone", 0x15971d40d91506cdull, -18.762935, 0.407637805 },
    { "noise", 0x8d1d44d1d46cf892ull, -17.627340, 0.505658925 },
    { "amplitude_steps", 0xf9d321b19163b153ull, -18.108441, 0.702545226 },
    { "parameter_steps", 0xb4d41de18bcd6b80ull, -18.027003, 0.505658925 },
    { "block_edge_transients", 0x808760cde838eec2ull, -32.744135, 0.707945764 },
    { "sustained_10s", 0x120b4b0037fd077dull, -15.745061, 0.428376436 },
    { "settings_minimum", 0x901fbf5754ce762dull, -16.813794, 0.251188636 },
    { "settings_default", 0xcf99f74f7598c4baull, -15.270659, 0.321255505 },
    { "settings_maximum", 0xa50c489140f1f3eeull, -18.957646, 0.251188636 },
    };
}
