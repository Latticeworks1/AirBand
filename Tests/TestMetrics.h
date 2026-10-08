#pragma once

#include <cstdint>
#include <vector>

// Reusable measurements. They know nothing about what counts as a pass.
namespace tests
{
    using Samples = std::vector<float>;

    double rmsDb (const Samples& samples, int startSample = 0);
    double peak (const Samples& samples, int startSample = 0);

    // Level in dB of the component at one frequency, from the correlation with a sine and a cosine over the samples
    // from startSample on.
    double toneDb (const Samples& samples, double frequencyHz, double sampleRate, int startSample = 0);
    double maxAbsDifference (const Samples& a, const Samples& b);
    bool allFinite (const Samples& samples);
    int countDifferent (const Samples& a, const Samples& b);

    // FNV-1a over the IEEE-754 bit patterns: equal iff every sample is bit-identical.
    std::uint64_t bitHash (const Samples& samples);
}
