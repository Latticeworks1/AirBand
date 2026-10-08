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

        section ("Dynamics 8: the compressor's attack and release are the same in dB at every level, and it does not distort a low tone");
        {
            const int rate = (int) kDefaultRate;
            auto on = transparentSettings();
            on.compAmount = 1.0f;

            // A 1 kHz tone steps from -30 dBFS to a louder level at 0.5 s and back at 1.5 s. The applied reduction is the
            // level change of the compressed tone relative to the uncompressed one, with the makeup gain taken off, read
            // from the peak of each millisecond.
            const auto reductionTrace = [&] (float stepDb)
            {
                const auto input = makeAmplitudeSteps (1000.0, { { 0.0316f, rate / 2 }, { (float) std::pow (10.0, stepDb / 20.0), rate }, { 0.0316f, rate } });
                const auto compressed = render (input, on);
                const auto plain = render (input, transparentSettings());
                const double makeup = VocalCompressor::gainReductionDb (VocalCompressor::kDesignReferenceDb, VocalCompressor::kDesignReferenceDb - VocalCompressor::kThresholdBelowReferenceDb, 4.0f);
                const int ms = rate / 1000;
                std::vector<double> trace;
                for (size_t start = (size_t) compressed.latency; start + (size_t) ms < compressed.output.size(); start += (size_t) ms)
                {
                    const Samples a (compressed.output.begin() + (long) start, compressed.output.begin() + (long) start + ms);
                    const Samples b (plain.output.begin() + (long) start, plain.output.begin() + (long) start + ms);
                    trace.push_back (makeup - (20.0 * std::log10 (peak (a) / std::max (peak (b), 1.0e-9))));
                }
                return trace;
            };

            // The traces start at the reported latency, where the first input sample emerges.
            const auto at = [] (const std::vector<double>& trace, double seconds) { return trace[(size_t) std::lround (seconds * 1000.0)]; };

            for (const float stepDb : { -10.0f, -4.0f })
            {
                const auto trace = reductionTrace (stepDb);
                const double settled = at (trace, 1.4);
                const double early = at (trace, 0.5 + 0.001), later = at (trace, 0.5 + 0.010);
                const double released = at (trace, 1.5 + 0.100), gone = at (trace, 1.5 + 0.800);
                check (settled > 1.0, "a step to " + std::to_string (stepDb) + " dBFS is reduced by " + std::to_string (settled) + " dB once settled");
                check (early <= 0.15 * settled && later >= 0.7 * settled, "the reduction builds in a few milliseconds: " + std::to_string (early / settled) + " of the final after 1 ms, "
                                                                           + std::to_string (later / settled) + " after 10 ms");
                check (released / settled >= 0.25 && released / settled <= 0.5 && gone <= 0.02 * settled, "and lets go over a few hundred: " + std::to_string (released / settled) + " of it remains after 100 ms, "
                                                                           + std::to_string (gone / settled) + " after 800 ms");
            }

            const auto remaining = [&] (float stepDb)
            {
                const auto trace = reductionTrace (stepDb);
                return at (trace, 1.6) / at (trace, 1.4);
            };
            check (std::abs (remaining (-10.0f) - remaining (-4.0f)) <= 0.05, "the fraction left 100 ms after the level falls is the same for a 20 dB step and a 26 dB step ("
                                                                                   + std::to_string (remaining (-10.0f)) + " and " + std::to_string (remaining (-4.0f)) + ")");

            // The harmonics (2nd to 8th) of a 100 Hz tone at -6 dBFS and of a 60 Hz tone, relative to the fundamental.
            const auto harmonicsDb = [&] (double frequency)
            {
                const auto output = render (makeSine (frequency, 0.5f, 4 * rate), on).output;
                const int cycles = (int) frequency;
                const int length = (int) std::lround ((double) cycles * kDefaultRate / frequency);
                const int start = 2 * rate;
                const Samples window (output.begin() + start, output.begin() + start + length);
                const double fundamental = std::pow (10.0, toneDb (window, frequency, kDefaultRate) / 10.0);
                double harmonics = 0.0;
                for (int k = 2; k <= 8; ++k)
                    harmonics += std::pow (10.0, toneDb (window, k * frequency, kDefaultRate) / 10.0);
                return 10.0 * std::log10 (harmonics / fundamental);
            };
            const double at100 = harmonicsDb (100.0), at60 = harmonicsDb (60.0);
            check (at100 <= -55.0, "the harmonics of a 100 Hz tone at -6 dBFS are " + std::to_string (at100) + " dB under it");
            check (at60 <= -48.0, "and those of a 60 Hz tone " + std::to_string (at60) + " dB under it");
        }
    }
}
