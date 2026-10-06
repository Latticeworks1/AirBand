#include <string>

#include "Check.h"
#include "Harness.h"
#include "Suites.h"
#include "TestMetrics.h"
#include "TestSettings.h"

namespace tests
{
    namespace
    {
        constexpr int kLength = (int) (2.0 * kDefaultRate);
        constexpr int kSettle = (int) (1.0 * kDefaultRate); // envelopes have settled after the first second

        AirBandSettings fullHighAir()
        {
            auto settings = transparentSettings();
            settings.highAirDb = 15.0f;
            settings.blend = 1.0f;
            return settings;
        }

        double gainDb (const Signal& input, const Signal& output)
        {
            return rmsDb (output, kSettle) - rmsDb (input, kSettle);
        }
    }

    void runAirTests()
    {
        section ("Air 1: quiet high-frequency content is boosted by High Air");
        {
            const auto input = makeSine (10000.0, 0.01f, kLength);
            const auto output = render (input, fullHighAir()).output;
            const double gain = gainDb (input, output);
            check (allFinite (output), "output contains no NaN/Inf");
            check (gain >= 3.0, "quiet tone gained at least +3 dB (measured " + std::to_string (gain) + " dB)");
            check (gain <= 16.0, "gain stayed within the +15 dB ceiling plus headroom (measured " + std::to_string (gain) + " dB)");
        }

        section ("Air 2: loud high-frequency content is left close to unity");
        {
            const auto input = makeSine (10000.0, 0.9f, kLength);
            const auto output = render (input, fullHighAir()).output;
            const double gain = gainDb (input, output);
            check (allFinite (output), "output contains no NaN/Inf");
            check (std::abs (gain) <= 1.5, "loud tone stayed within 1.5 dB of unity (measured " + std::to_string (gain) + " dB)");
        }

        section ("Air 3: De-Ess suppresses concentrated sibilant energy more than broadband energy");
        {
            auto settings = fullHighAir();
            settings.deEssAmount = 1.0f;
            const auto sibilantIn = makeSine (6500.0, 0.05f, kLength);
            const auto broadbandIn = makeMultiTone ({ 9500.0, 10500.0, 11500.0, 12500.0, 13500.0 }, 0.07f, kLength);
            const double sibilant = gainDb (sibilantIn, render (sibilantIn, settings).output);
            const double broadband = gainDb (broadbandIn, render (broadbandIn, settings).output);
            check (broadband - sibilant >= 2.0, "sibilant content gained less than broadband (sibilant " + std::to_string (sibilant)
                                                    + " dB, broadband " + std::to_string (broadband) + " dB)");
        }
    }
}
