#include <cmath>
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
        constexpr int kSettle = (int) (1.0 * kDefaultRate);

        // Level change in dB caused by switching one stage from off to on, at a given input level.
        double stageDelta (float amplitude, AirBandSettings on)
        {
            const auto input = makeSine (1000.0, amplitude, kLength);
            return rmsDb (render (input, on).output, kSettle) - rmsDb (render (input, transparentSettings()).output, kSettle);
        }
    }

    void runDynamicsTests()
    {
        section ("Dynamics 1: compressor reduces gain on loud content relative to quiet content");
        {
            auto on = transparentSettings();
            on.compAmount = 1.0f;
            const double quiet = stageDelta (0.02f, on);
            const double loud = stageDelta (0.9f, on);
            check (std::abs (quiet - 6.0) <= 0.5, "quiet content gets the full +6 dB makeup gain (measured " + std::to_string (quiet) + " dB)");
            check (quiet - loud >= 2.0, "loud content gains less once compressed (quiet " + std::to_string (quiet)
                                            + " dB, loud " + std::to_string (loud) + " dB)");
        }

        section ("Dynamics 2: gate attenuates quiet content and leaves loud content alone");
        {
            auto on = transparentSettings();
            on.gateAmount = 1.0f;
            const double quiet = stageDelta (0.005f, on);
            const double loud = stageDelta (0.5f, on);
            check (quiet <= -3.0, "quiet content is attenuated (measured " + std::to_string (quiet) + " dB)");
            check (std::abs (loud) <= 0.5, "loud content is left alone (measured " + std::to_string (loud) + " dB)");
        }

        section ("Dynamics 3: limiter keeps the output peak at or under its ceiling");
        {
            auto settings = transparentSettings();
            settings.limiterCeilingDb = -6.0f;
            const auto output = render (makeSine (1000.0, 1.0f, kLength), settings).output;
            const double ceiling = std::pow (10.0, -6.0 / 20.0);
            check (allFinite (output), "output contains no NaN/Inf");
            check (peak (output, kSettle) <= ceiling * 1.05, "output peak stayed under the -6 dB ceiling (measured "
                                                                  + std::to_string (peak (output, kSettle)) + ")");
        }
    }
}
