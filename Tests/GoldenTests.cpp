#include <cmath>
#include <string>

#include "Check.h"
#include "GoldenCases.h"
#include "GoldenVectors.h"
#include "Suites.h"
#include "TestMetrics.h"

// Output of the production DSP for fixed stimuli. The bit-exact hash is checked on the platform
// that recorded it, in a release build; elsewhere compilers may fuse or reorder float arithmetic,
// so only RMS and peak are compared. Their tolerances come from an observed divergence: rebuilding
// with -ffp-contract=off (no fused multiply-add, as on MSVC x64) changes 11 of 12 hashes and moves
// RMS by at most 4.9e-5 dB (the 10 s case) and peak by at most 7.4e-8 relative. The tolerances are
// 5e-4 dB and 1e-6 relative, roughly ten times those drifts. Re-measure when a new platform joins CI.
namespace tests
{
    namespace
    {
        constexpr bool kBitExactPlatform =
#if defined(__APPLE__) && defined(__aarch64__) && defined(NDEBUG)
            true;
#else
            false;
#endif

        const GoldenVector* find (const std::string& name)
        {
            for (const auto& vector : kGoldenVectors)
                if (name == vector.name)
                    return &vector;
            return nullptr;
        }
    }

    void runGoldenTests (bool record)
    {
        if (! record)
            section ("Golden: output matches the recorded behaviour for every stimulus");

        for (const auto& goldenCase : goldenCases())
        {
            const auto output = render (goldenCase.input, goldenCase.settings, goldenCase.options).output;
            const double rms = rmsDb (output);
            const double top = peak (output);

            if (record)
            {
                std::printf ("    { \"%s\", 0x%016llxull, %.6f, %.9g },\n", goldenCase.name.c_str(),
                             (unsigned long long) bitHash (output), rms, top);
                continue;
            }

            const auto* expected = find (goldenCase.name);
            check (expected != nullptr, goldenCase.name + ": has a recorded vector");
            if (expected == nullptr)
                continue;

            check (allFinite (output), goldenCase.name + ": output is finite");
            check (std::abs (rms - expected->rmsDb) <= 5.0e-4 && std::abs (top - expected->peak) <= 1.0e-6 * expected->peak + 1.0e-9,
                   goldenCase.name + ": rms " + std::to_string (rms) + " dB (drift " + scientific (rms - expected->rmsDb) + "), peak "
                       + std::to_string (top) + " (relative drift " + scientific ((top - expected->peak) / expected->peak) + ")");
            if (kBitExactPlatform)
                check (bitHash (output) == expected->hash, goldenCase.name + ": bit-identical to the recording");
        }
    }
}
