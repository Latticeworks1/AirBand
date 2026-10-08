#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "AirBandDSP.h"
#include "Check.h"
#include "EnvelopeDetector.h"
#include "Harness.h"
#include "LevelTracker.h"
#include "SibilanceDetector.h"
#include "Suites.h"
#include "TestMetrics.h"
#include "TestSettings.h"
#include "VocalCompressor.h"

namespace tests
{
    namespace
    {
        constexpr double kRate = kDefaultRate;

        float gainOf (double db) { return (float) std::pow (10.0, db / 20.0); }

        // Feeds a constant envelope to a tracker for the given time and returns the reference it ends on.
        float settle (LevelTracker& tracker, double levelDb, double seconds)
        {
            const float level = gainOf (levelDb);
            for (int i = 0; i < (int) (seconds * kRate); ++i)
                tracker.push (level);
            return tracker.referenceDb();
        }

        // Mean of (output - input) frame levels, in dB, over the loudest `fraction` of the 100 ms frames after `from` seconds,
        // ranked by the input's own frame level. The output is read at the reported latency.
        double loudestChange (const Signal& input, const Render& result, double fromSeconds, double fraction)
        {
            const int frame = (int) (0.1 * kRate);
            std::vector<std::pair<double, double>> frames; // (input level, change)
            for (int start = (int) (fromSeconds * kRate); start + frame + result.latency <= (int) result.output.size(); start += frame)
            {
                // Skip the first 30 ms of each burst, in which the detectors are still settling on its level.
                const int skip = (int) (0.03 * kRate);
                const Samples in (input.begin() + start + skip, input.begin() + start + frame);
                const Samples out (result.output.begin() + start + skip + result.latency, result.output.begin() + start + frame + result.latency);
                frames.emplace_back (rmsDb (in), rmsDb (out) - rmsDb (in));
            }
            std::sort (frames.begin(), frames.end());
            double total = 0.0;
            const size_t from = (size_t) ((double) frames.size() * (1.0 - fraction));
            for (size_t i = from; i < frames.size(); ++i)
                total += frames[i].second;
            return total / (double) (frames.size() - from);
        }

        Signal scaled (Signal signal, double db)
        {
            const float g = gainOf (db);
            for (auto& v : signal)
                v *= g;
            return signal;
        }
    }

    void runTrackingTests()
    {
        section ("Tracking 1: the level tracker settles on the level of steady material, ignores silence, and holds the design level when off");
        {
            LevelTracker tracker;
            tracker.prepare (kRate, -20.0f);
            check (std::abs (tracker.referenceDb() - -20.0f) < 1.0e-6f, "the reference starts at the design level");

            const float settled = settle (tracker, -30.0, 90.0);
            check (std::abs (settled - -30.0f) <= 1.0f, "a steady -30 dB level is tracked to within the 1 dB bin width (reference " + std::to_string (settled) + " dB)");

            const float afterSilence = settle (tracker, -100.0, 60.0);
            check (std::abs (afterSilence - settled) <= 0.01f, "silence below the floor leaves the reference where it was (" + std::to_string (afterSilence) + " dB)");

            tracker.setTracking (false);
            const float off = settle (tracker, -30.0, 2.0);
            check (std::abs (off - -20.0f) <= 0.05f, "with tracking off the reference returns to the design level (" + std::to_string (off) + " dB)");

            LevelTracker clamped;
            clamped.prepare (kRate, -20.0f);
            const float low = settle (clamped, -80.0, 90.0);
            check (low >= -20.0f - LevelTracker::kWindowDb - 0.01f, "the reference stays inside its window of " + std::to_string ((int) LevelTracker::kWindowDb)
                                                                           + " dB around the design level (" + std::to_string (low) + " dB)");
        }

        section ("Tracking 2: the air knees follow the level of the material");
        {
            // Bursts of noise at levels spread over 25 dB, 60 s long; High Air at full boost and blend. The loudest third of
            // the bursts keeps the same lift when the whole signal is 12 dB quieter if the knee follows the level, and gains
            // much more lift when the knee is fixed.
            const auto program = makeLevelBursts ((int) (60.0 * kRate), (int) (0.1 * kRate), -55.0, -30.0, 3u);
            const auto quieter = scaled (program, -12.0);

            auto settings = transparentSettings();
            settings.highAirDb = 15.0f;
            settings.blend = 1.0f;

            const auto lift = [&] (const Signal& signal, bool tracking)
            {
                auto on = settings;
                on.levelTracking = tracking;
                auto off = transparentSettings();
                off.levelTracking = tracking;
                return loudestChange (signal, render (signal, on), 30.0, 1.0 / 3.0) - loudestChange (signal, render (signal, off), 30.0, 1.0 / 3.0);
            };

            const double followedFull = lift (program, true), followedQuiet = lift (quieter, true);
            const double fixedFull = lift (program, false), fixedQuiet = lift (quieter, false);
            check (std::abs (followedFull - followedQuiet) <= 1.0, "tracking on: the lift of the loudest third differs by "
                                                                       + std::to_string (std::abs (followedFull - followedQuiet)) + " dB between the signal and a copy 12 dB quieter ("
                                                                       + std::to_string (followedFull) + " and " + std::to_string (followedQuiet) + " dB)");
            check (std::abs (fixedFull - fixedQuiet) >= 2.5, "tracking off: the same copies differ by "
                                                                 + std::to_string (std::abs (fixedFull - fixedQuiet)) + " dB (" + std::to_string (fixedFull) + " and " + std::to_string (fixedQuiet) + " dB)");
        }

        section ("Tracking 3: the compressor threshold follows the level of the material");
        {
            const auto program = makeLevelBursts ((int) (60.0 * kRate), (int) (0.1 * kRate), -30.0, -5.0, 5u);
            const auto quieter = scaled (program, -12.0);

            auto settings = transparentSettings();
            settings.compAmount = 1.0f;

            const auto change = [&] (const Signal& signal, bool tracking)
            {
                auto on = settings;
                on.levelTracking = tracking;
                return loudestChange (signal, render (signal, on), 30.0, 0.1);
            };

            const double followedFull = change (program, true), followedQuiet = change (quieter, true);
            const double fixedFull = change (program, false), fixedQuiet = change (quieter, false);
            check (std::abs (followedFull - followedQuiet) <= 1.0, "tracking on: the loudest tenth changes by " + std::to_string (followedFull) + " and "
                                                                       + std::to_string (followedQuiet) + " dB for the signal and a copy 12 dB quieter");
            check (std::abs (fixedFull - fixedQuiet) >= 3.0, "tracking off: the same copies change by " + std::to_string (fixedFull) + " and "
                                                                 + std::to_string (fixedQuiet) + " dB");
            check (followedFull <= -0.5, "the compressor lowers the loudest tenth, the part above its reference (" + std::to_string (followedFull) + " dB)");
        }

        section ("Tracking 4: the compressor's gain computer has a soft knee, unity gain at its reference, and is transparent at 1:1");
        {
            const float threshold = -30.0f;
            check (VocalCompressor::gainReductionDb (-40.0f, threshold, 4.0f) == 0.0f, "no reduction below the knee");
            check (VocalCompressor::gainReductionDb (-30.0f, threshold, 4.0f) > 0.0f
                       && VocalCompressor::gainReductionDb (-30.0f, threshold, 4.0f) < 0.75f * 6.0f / 8.0f + 1.0e-4f,
                   "at the threshold the reduction is partway into the knee (" + std::to_string (VocalCompressor::gainReductionDb (-30.0f, threshold, 4.0f)) + " dB)");
            const float top = threshold + 0.5f * VocalCompressor::kKneeDb;
            check (std::abs (VocalCompressor::gainReductionDb (top + 0.001f, threshold, 4.0f) - VocalCompressor::gainReductionDb (top - 0.001f, threshold, 4.0f)) < 0.01f,
                   "the reduction is continuous where the knee meets the straight section");
            check (std::abs (VocalCompressor::gainReductionDb (-10.0f, threshold, 4.0f) - 0.75f * 20.0f) < 1.0e-4f, "20 dB over the threshold at 4:1 is reduced by 15 dB");
            check (VocalCompressor::gainReductionDb (-10.0f, threshold, 1.0f) == 0.0f, "1:1 never reduces");

            // Held at the design reference, a level there passes at unity gain (the reduction at the reference is the makeup).
            // The follower's reading of a sine is found by running the follower itself.
            EnvelopeDetector follower;
            follower.prepare (kRate, 0.005f, 0.005f, 0.1f);
            const auto probe = makeSine (1000.0, 0.1f, (int) (3.0 * kRate));
            double reading = 0.0;
            for (size_t i = 0; i < probe.size(); ++i)
            {
                const float level = follower.pushSample (std::abs (probe[i]));
                if (i >= probe.size() - 441)
                    reading += (double) level / 441.0;
            }

            auto settings = transparentSettings();
            settings.compAmount = 1.0f;
            const double amplitude = 0.1 * std::pow (10.0, (double) VocalCompressor::kDesignReferenceDb / 20.0) / reading;
            const auto input = makeSine (1000.0, (float) amplitude, (int) (2.0 * kRate));
            const double delta = rmsDb (render (input, settings).output, (int) kRate) - rmsDb (render (input, transparentSettings()).output, (int) kRate);
            check (std::abs (delta) <= 0.3, "a tone whose follower reading is the design reference passes within 0.3 dB of unity gain (" + std::to_string (delta) + " dB)");
        }

        section ("Tracking 5: the sibilance measure separates fricative noise from voiced content and does not depend on level");
        {
            const auto filtered = [] (const Signal& noise, bool bandpass)
            {
                using Coefficients = juce::dsp::IIR::Coefficients<float>;
                juce::dsp::IIR::Filter<float> a, b;
                a.coefficients = bandpass ? Coefficients::makeBandPass (kRate, 6500.0f, 1.2f) : Coefficients::makeLowPass (kRate, 1500.0f);
                b.coefficients = a.coefficients;
                Signal out (noise.size());
                for (size_t i = 0; i < noise.size(); ++i)
                    out[i] = b.processSample (a.processSample (noise[i]));
                return out;
            };

            const auto meanSibilance = [] (const Signal& signal)
            {
                SibilanceDetector detector;
                detector.prepare (kRate);
                double total = 0.0;
                for (size_t i = 0; i < signal.size(); ++i)
                {
                    const float value = detector.process (signal[i]);
                    if (i >= (size_t) kRate)
                        total += value;
                }
                return total / (double) (signal.size() - (size_t) kRate);
            };

            const auto noise = makeNoise ((int) (3.0 * kRate), 0.5f, 7u);
            const double fricative = meanSibilance (filtered (noise, true));
            const double voiced = meanSibilance (filtered (noise, false));
            const double quietFricative = meanSibilance (scaled (filtered (noise, true), -50.0));
            const double white = meanSibilance (noise);
            check (fricative >= 0.8, "noise between 5 and 8 kHz reads as sibilant (" + std::to_string (fricative) + ")");
            check (voiced <= 0.05, "noise below 1.5 kHz reads as not sibilant (" + std::to_string (voiced) + ")");
            check (std::abs (quietFricative - fricative) <= 0.05, "the reading does not depend on level (" + std::to_string (quietFricative) + " at -50 dB)");
            check (white >= 0.5, "flat noise, which has as much level in the band as a fricative of its width, reads as sibilant (" + std::to_string (white) + ")");
        }

        section ("Tracking 6: the low-level gain function reproduces the response of the DSP");
        {
            struct Case { float mid, high, blend; };
            double worst = 0.0;
            for (const Case c : { Case { 15.0f, 0.0f, 1.0f }, Case { 0.0f, 15.0f, 0.2f }, Case { 9.0f, 9.0f, 0.4f }, Case { 15.0f, 15.0f, 0.2f } })
            {
                auto settings = transparentSettings();
                settings.midAirDb = c.mid;
                settings.highAirDb = c.high;
                settings.blend = c.blend;
                for (const double frequency : { 500.0, 2000.0, 3000.0, 4500.0, 6000.0, 9000.0, 12000.0, 16000.0 })
                {
                    const auto input = makeSine (frequency, 1.0e-4f, (int) (2.0 * kRate));
                    const double measured = rmsDb (render (input, settings).output, (int) kRate) - rmsDb (render (input, transparentSettings()).output, (int) kRate);
                    const double predicted = AirBandDSP::lowLevelGainDb (settings, frequency, kRate);
                    worst = std::max (worst, std::abs (measured - predicted));
                }
            }
            check (worst <= 0.15, "the predicted and the measured gain agree within 0.15 dB at every frequency tried (worst " + std::to_string (worst) + " dB)");
        }
    }
}
