#include <algorithm>
#include <cmath>
#include <string>

#include "Check.h"
#include "Harness.h"
#include "Suites.h"
#include "TestMetrics.h"
#include "TestSettings.h"

namespace tests
{
    // With every effect neutral the plugin must pass audio unchanged beyond its reported lookahead
    // delay: this is the direct check against a signal path that compiles and runs but is dead.
    static void transparentPassThrough()
    {
        section ("Linear 1: transparent settings pass audio through unchanged beyond the reported latency");
        const int length = (int) (2.0 * kDefaultRate);
        const auto input = makeSine (1000.0, 0.25f, length);
        const auto result = render (input, transparentSettings());

        double maxDiff = 0.0;
        for (int i = result.latency; i < length; ++i)
            maxDiff = std::max (maxDiff, (double) std::abs (result.output[(size_t) i] - input[(size_t) (i - result.latency)]));

        check (allFinite (result.output), "output contains no NaN/Inf");
        check (result.latency > 0, "limiter lookahead is reported as latency (" + std::to_string (result.latency) + " samples)");
        check (maxDiff < 1.0e-5, "output equals the delayed input (max diff " + std::to_string (maxDiff) + ")");
    }

    // An impulse through the transparent chain is a delayed, unscaled impulse: the delay equals the
    // reported latency exactly, and nothing else rings.
    static void impulseResponse()
    {
        section ("Linear 2: impulse response is a single unit impulse at the reported latency");
        const auto result = render (makeImpulse (4096, 100, 0.5f), transparentSettings());
        const size_t at = (size_t) (100 + result.latency);

        double elsewhere = 0.0;
        for (size_t i = 0; i < result.output.size(); ++i)
            if (i != at)
                elsewhere = std::max (elsewhere, (double) std::abs (result.output[i]));

        check (std::abs (result.output[at] - 0.5f) < 1.0e-6f, "impulse arrives at sample " + std::to_string (at) + " with unit gain");
        check (elsewhere < 1.0e-6, "no other sample deviates from silence (max " + std::to_string (elsewhere) + ")");
    }

    // Latency is the 5 ms limiter lookahead, which follows the sample rate, plus the whole-sample delay that
    // lines the dry signal up with the air path; that delay is set by the oversampling filter, not by the rate.
    static void latencyTracksSampleRate()
    {
        section ("Linear 3: reported latency is the 5 ms lookahead plus a fixed alignment delay, and is the true delay at each sample rate");
        int alignment = -1;
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            RenderOptions options;
            options.sampleRate = rate;
            const auto result = render (makeImpulse (4096, 100, 0.5f), transparentSettings(), options);
            const int lookahead = (int) std::round (0.005 * rate);
            const int extra = result.latency - lookahead;
            if (alignment < 0)
                alignment = extra;

            check (extra > 0 && extra == alignment, "latency at " + std::to_string ((int) rate) + " Hz is " + std::to_string (result.latency)
                                                        + " samples: " + std::to_string (lookahead) + " lookahead plus " + std::to_string (extra) + " alignment");
            check (std::abs (result.output[(size_t) (100 + result.latency)] - 0.5f) < 1.0e-6f, "the impulse arrives at the reported latency at "
                                                                                                    + std::to_string ((int) rate) + " Hz");
        }
    }

    void runLinearTests()
    {
        transparentPassThrough();
        impulseResponse();
        latencyTracksSampleRate();
    }
}
