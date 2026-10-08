#include "TestMetrics.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tests
{
    double rmsDb (const Samples& samples, int startSample)
    {
        double sumSq = 0.0;
        for (size_t i = (size_t) startSample; i < samples.size(); ++i)
            sumSq += (double) samples[i] * (double) samples[i];

        const double count = (double) std::max<size_t> (1, samples.size() - (size_t) startSample);
        return 20.0 * std::log10 (std::max (std::sqrt (sumSq / count), 1.0e-12));
    }

    double peak (const Samples& samples, int startSample)
    {
        double result = 0.0;
        for (size_t i = (size_t) startSample; i < samples.size(); ++i)
            result = std::max (result, (double) std::abs (samples[i]));
        return result;
    }

    double toneDb (const Samples& samples, double frequencyHz, double sampleRate, int startSample)
    {
        double sine = 0.0, cosine = 0.0;
        const double step = 2.0 * 3.14159265358979323846 * frequencyHz / sampleRate;
        for (size_t i = (size_t) startSample; i < samples.size(); ++i)
        {
            sine += (double) samples[i] * std::sin (step * (double) i);
            cosine += (double) samples[i] * std::cos (step * (double) i);
        }

        const double count = (double) std::max<size_t> (1, samples.size() - (size_t) startSample);
        return 20.0 * std::log10 (std::max (2.0 * std::sqrt (sine * sine + cosine * cosine) / count, 1.0e-12));
    }

    double maxAbsDifference (const Samples& a, const Samples& b)
    {
        double result = 0.0;
        for (size_t i = 0; i < std::min (a.size(), b.size()); ++i)
            result = std::max (result, (double) std::abs (a[i] - b[i]));
        return result;
    }

    bool allFinite (const Samples& samples)
    {
        return std::all_of (samples.begin(), samples.end(), [] (float v) { return std::isfinite (v); });
    }

    int countDifferent (const Samples& a, const Samples& b)
    {
        if (a.size() != b.size())
            return (int) std::max (a.size(), b.size());

        int count = 0;
        for (size_t i = 0; i < a.size(); ++i)
            count += std::memcmp (&a[i], &b[i], sizeof (float)) != 0 ? 1 : 0;
        return count;
    }

    std::uint64_t bitHash (const Samples& samples)
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (float sample : samples)
        {
            std::uint32_t bits = 0;
            std::memcpy (&bits, &sample, sizeof bits);
            for (int byte = 0; byte < 4; ++byte)
                hash = (hash ^ ((bits >> (8 * byte)) & 0xffu)) * 1099511628211ull;
        }
        return hash;
    }
}
