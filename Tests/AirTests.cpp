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
            // -54 dBFS, well under the 9 kHz band's knee, which sits above the band's median level on speech.
            const auto input = makeSine (10000.0, 0.002f, kLength);
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

        section ("Air 3: De-Ess withdraws the High Air boost when sibilant energy accompanies the band's content, and never cuts below the dry level");
        {
            // De-Ess withdraws boost where the sibilance measure (the 6 kHz band's share of the whole signal) is high, so
            // the boost of the band's own content (broadband air here, measured at 12 kHz) is lost when sibilant energy
            // (6.5 kHz, which the band's high-pass mostly rejects) is present. Both stimuli sit under the band's knee,
            // where the boost is active.
            const auto air = makeMultiTone ({ 10500.0, 12000.0, 13500.0, 15000.0 }, 0.002f, kLength);
            auto withSibilant = makeSine (6500.0, 0.006f, kLength);
            for (size_t i = 0; i < withSibilant.size(); ++i)
                withSibilant[i] += air[i];

            auto off = fullHighAir();
            off.deEssAmount = 0.0f;
            auto on = off;
            on.deEssAmount = 1.0f;

            const auto bite = [&] (const Signal& input)
            {
                return toneDb (render (input, on).output, 12000.0, kDefaultRate, kSettle) - toneDb (render (input, off).output, 12000.0, kDefaultRate, kSettle);
            };

            const double alone = bite (air);
            const double accompanied = bite (withSibilant);
            check (accompanied <= alone - 3.0, "De-Ess lowers the 12 kHz boost by " + std::to_string (-accompanied) + " dB with sibilant energy present, "
                                                   + std::to_string (-alone) + " dB without");

            // At full De-Ess the boost is withdrawn, not turned into a cut: the 12 kHz level stays at the dry level.
            auto dry = off;
            dry.highAirDb = 0.0f;
            const double against = toneDb (render (withSibilant, on).output, 12000.0, kDefaultRate, kSettle) - toneDb (render (withSibilant, dry).output, 12000.0, kDefaultRate, kSettle);
            check (against >= -0.3, "at full De-Ess the 12 kHz level is within 0.3 dB of the dry level or above it (" + std::to_string (against) + " dB)");
        }

        // The air path is summed with the dry signal, so any delay between the two turns the sum into a
        // comb filter (a 3-sample misalignment gives a 17 dB notch near 8 kHz). At the default blend the
        // low-level response must therefore stay near or above unity at every frequency. The phase lead of
        // the band's high-pass leaves an unavoidable dip of about 1.2 to 1.4 dB just below its corner.
        section ("Air 4: the air path adds to the dry path without cancelling it at any frequency");
        for (const bool mid : { true, false })
        {
            auto settings = transparentSettings();
            (mid ? settings.midAirDb : settings.highAirDb) = 15.0f;

            double worst = 1.0e9, worstAt = 0.0, best = -1.0e9;
            for (const double frequency : { 200.0, 500.0, 1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0, 8000.0, 9000.0,
                                            10000.0, 12000.0, 14000.0, 16000.0 })
            {
                const auto input = makeSine (frequency, 1.0e-4f, kLength);
                const double change = rmsDb (render (input, settings).output, kSettle) - rmsDb (render (input, transparentSettings()).output, kSettle);
                if (change < worst)
                {
                    worst = change;
                    worstAt = frequency;
                }
                best = std::max (best, change);
            }

            check (worst >= -2.0, std::string (mid ? "Mid" : "High") + " Air never cuts by more than 2 dB (worst " + std::to_string (worst)
                                      + " dB at " + std::to_string ((int) worstAt) + " Hz)");
            check (best >= 2.0, std::string (mid ? "Mid" : "High") + " Air boosts somewhere by at least 2 dB (best " + std::to_string (best) + " dB)");
        }
    }
}
