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
    { "impulse", 0x42b9656f028f728bull, -46.534574, 0.516241014 },
    { "silence_to_transient", 0x988e7947993fac37ull, -29.867883, 0.448847532 },
    { "steady_sine", 0x3872628fe36b500eull, -17.559692, 0.231864393 },
    { "two_tone", 0x3e1573866472a84cull, -15.994716, 0.370898694 },
    { "noise", 0x2cb73815e451d7c8ull, -17.559826, 0.357539922 },
    { "amplitude_steps", 0x6bfbfc41b1797b7cull, -19.367169, 0.450278163 },
    { "amplitude_steps_pinned", 0x29e50eff9eb97f6dull, -20.510485, 0.460651457 },
    { "parameter_steps", 0x46a99edf12a94acfull, -17.777571, 0.357539922 },
    { "transport_events", 0x9b98c9ce73925975ull, -19.567300, 0.499007255 },
    { "block_edge_transients", 0xcb4e406ddcc39f42ull, -35.302761, 0.706088126 },
    { "sustained_10s", 0x74ad19d023330497ull, -15.201857, 0.31512928 },
    { "settings_minimum", 0x40febdc4588e03ecull, -16.817773, 0.251188636 },
    { "settings_default", 0x8b1ff7f46521690bull, -15.284611, 0.299996436 },
    { "settings_maximum", 0x31fb5580c1721149ull, -17.127749, 0.251188636 },
    };
}
