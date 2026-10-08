#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "AirBandDSP.h"
#include "Check.h"
#include "Harness.h"
#include "Suites.h"
#include "TestMetrics.h"
#include "TestSettings.h"
#include "VocalCompressor.h"

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
        section ("Dynamics 1: compressor reduces gain on loud content and raises content under its knee by the makeup gain");
        {
            auto on = transparentSettings();
            on.compAmount = 1.0f;
            const double quiet = stageDelta (0.02f, on);
            const double loud = stageDelta (0.9f, on);
            const double reference = VocalCompressor::kDesignReferenceDb;
            const double makeup = VocalCompressor::gainReductionDb ((float) reference, (float) reference - VocalCompressor::kThresholdBelowReferenceDb, 4.0f);
            check (std::abs (quiet - makeup) <= 0.3, "quiet content gets the makeup gain, the reduction at the reference level (measured "
                                                         + std::to_string (quiet) + " dB, expected " + std::to_string (makeup) + " dB)");
            check (quiet - loud >= 6.0, "loud content gains less once compressed (quiet " + std::to_string (quiet)
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

        section ("Dynamics 4: the gate holds briefly after a phrase, then closes toward its full reduction");
        {
            const int rate = (int) kDefaultRate;
            // A -20 dBFS phrase for one second, then a -50 dBFS tail, which at full amount settles near 7.8 dB of reduction.
            const auto input = makeAmplitudeSteps (1000.0, { { 0.1f, rate }, { 0.00316f, 2 * rate } });
            auto on = transparentSettings();
            on.gateAmount = 1.0f;
            const auto result = render (input, on);
            const auto reference = render (input, transparentSettings()).output;

            const auto attenuationDb = [&] (double fromMs)
            {
                const size_t from = (size_t) (rate + result.latency + (int) (fromMs * 0.001 * rate));
                const size_t length = (size_t) (0.02 * rate);
                const Samples gated (result.output.begin() + (long) from, result.output.begin() + (long) (from + length));
                const Samples plain (reference.begin() + (long) from, reference.begin() + (long) (from + length));
                return rmsDb (gated) - rmsDb (plain);
            };

            const double early = attenuationDb (0.0), mid = attenuationDb (250.0), settled = attenuationDb (1500.0);
            check (early >= -1.0, "the first 20 ms after the phrase is held open (" + std::to_string (early) + " dB)");
            check (mid <= -5.0, "250 ms after the phrase the gate is most of the way closed (" + std::to_string (mid) + " dB)");
            check (settled <= -6.5 && settled >= -9.0, "the tail settles near the expander law's 7.8 dB (" + std::to_string (settled) + " dB)");
        }

        section ("Dynamics 5: the gate threshold sets the level below which content is attenuated");
        {
            auto on = transparentSettings();
            on.gateAmount = 1.0f;
            on.gateThresholdDb = -50.0f;
            const double under = stageDelta (0.00562f, on); // -45 dBFS, above a -50 dBFS threshold
            on.gateThresholdDb = -30.0f;
            const double over = stageDelta (0.00562f, on);  // -45 dBFS, below a -30 dBFS threshold: 15 dB under at 4:1
            check (std::abs (under) <= 0.5, "content above the threshold is left alone (" + std::to_string (under) + " dB)");
            check (over <= -8.0, "content below the threshold is attenuated (" + std::to_string (over) + " dB)");
        }

        section ("Dynamics 6: low-frequency rumble does not hold the gate open");
        {
            auto on = transparentSettings();
            on.gateAmount = 1.0f;
            const auto voice = makeSine (1000.0, 0.00316f, kLength); // -50 dBFS
            auto withRumble = makeSine (60.0, 0.00562f, kLength);    // -45 dBFS
            for (size_t i = 0; i < withRumble.size(); ++i)
                withRumble[i] += voice[i];

            const auto voiceChange = [&] (const Signal& input)
            {
                return toneDb (render (input, on).output, 1000.0, kDefaultRate, kSettle) - toneDb (render (input, transparentSettings()).output, 1000.0, kDefaultRate, kSettle);
            };

            const double alone = voiceChange (voice);
            const double accompanied = voiceChange (withRumble);
            check (accompanied - alone <= 3.5, "rumble 5 dB louder than the voice lessens its attenuation by " + std::to_string (accompanied - alone)
                                                   + " dB (alone " + std::to_string (alone) + " dB, with rumble " + std::to_string (accompanied) + " dB)");
        }

        section ("Dynamics 7: the gate is stereo linked, so a quiet channel is not gated under a loud one");
        {
            AirBandDSP dsp;
            dsp.prepare (kDefaultRate, 512, 2);
            auto on = transparentSettings();
            on.gateAmount = 1.0f;
            dsp.setParameters (on);

            const auto loud = makeSine (1000.0, 0.1f, kLength);       // -20 dBFS
            const auto quiet = makeSine (1000.0, 0.00316f, kLength);  // -50 dBFS: gated by 7.8 dB if it stood alone
            juce::AudioBuffer<float> buffer (2, 512);
            Signal right;
            for (int start = 0; start < kLength; start += 512)
            {
                const int length = std::min (512, kLength - start);
                buffer.setSize (2, length, false, false, true);
                buffer.copyFrom (0, 0, loud.data() + start, length);
                buffer.copyFrom (1, 0, quiet.data() + start, length);
                dsp.processBlock (buffer);
                right.insert (right.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + length);
            }

            const double change = toneDb (right, 1000.0, kDefaultRate, kSettle) - toneDb (quiet, 1000.0, kDefaultRate, kSettle);
            check (std::abs (change) <= 0.5, "the quiet channel keeps its level while the other channel is loud (" + std::to_string (change) + " dB)");
        }
    }
}
