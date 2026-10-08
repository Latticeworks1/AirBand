#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "AirBandDSP.h"
#include "Check.h"
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

        // Feeds 10 ms levels of base - 12 u dB, u pseudo-random in [0, 1), for the given time; the reference ends near base - 1.2 dB.
        float playIrregular (LevelTracker& tracker, double baseDb, double seconds, std::uint32_t& state)
        {
            for (int k = 0; k < (int) (seconds * 100.0); ++k)
            {
                state = state * 1664525u + 1013904223u;
                const float level = gainOf (baseDb - 12.0 * (double) (state >> 8) / 16777216.0);
                for (int i = 0; i < 441; ++i)
                    tracker.push (level);
            }
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

            // Held at the design reference, a tone whose peak is at that level passes at unity gain (the reduction at the
            // reference is the makeup).
            auto settings = transparentSettings();
            settings.compAmount = 1.0f;
            const auto input = makeSine (1000.0, gainOf (VocalCompressor::kDesignReferenceDb), (int) (2.0 * kRate));
            const double delta = rmsDb (render (input, settings).output, (int) kRate) - rmsDb (render (input, transparentSettings()).output, (int) kRate);
            check (std::abs (delta) <= 0.3, "a tone whose peak is the design reference passes within 0.3 dB of unity gain (" + std::to_string (delta) + " dB)");
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

        section ("Tracking 7: after a restart the reference holds, then moves to the new level within seconds, and a new signal is acquired as fast");
        {
            LevelTracker restarted, control;
            restarted.prepare (kRate, -40.0f);
            control.prepare (kRate, -40.0f);
            settle (restarted, -30.0, 60.0);
            settle (control, -30.0, 60.0);

            restarted.restart();
            const float before = restarted.referenceDb();
            const float level = gainOf (-42.0);
            restarted.push (level);
            for (int i = 1; i < (int) (0.01 * kRate); ++i)
                restarted.push (level);
            check (std::abs (restarted.referenceDb() - before) <= 0.1f, "the reference does not step when the tracker restarts (moved "
                                                                           + std::to_string (restarted.referenceDb() - before) + " dB in the first update)");

            const float after = settle (restarted, -42.0, 1.0), stale = settle (control, -42.0, 1.0);
            check (std::abs (after - -42.0f) <= 1.5f, "one second after the restart the reference is within 1.5 dB of the new level (" + std::to_string (after) + " dB)");
            check (stale >= -34.0f, "a tracker that is not restarted is still near the old level at that time (" + std::to_string (stale) + " dB)");

            LevelTracker fresh;
            fresh.prepare (kRate, -30.0f);
            const float started = settle (fresh, -45.0, 1.0);
            check (std::abs (started - -45.0f) <= 1.5f, "a tracker that starts on material 15 dB under its design level reaches it within 1.5 dB in one second (" + std::to_string (started) + " dB)");
        }

        section ("Tracking 8: levels far under the reference are not activity, except while acquiring after a restart");
        {
            LevelTracker tracker;
            tracker.prepare (kRate, -40.0f);
            const float settled = settle (tracker, -30.0, 30.0);
            const float paused = settle (tracker, -62.0, 90.0);
            check (std::abs (paused - settled) <= 0.1f, "90 s at a level 32 dB under the reference leave it where it was (" + std::to_string (settled) + " to " + std::to_string (paused) + " dB)");
            const float resumed = settle (tracker, -30.0, 0.5);
            check (std::abs (resumed - settled) <= 0.5f, "and when the material returns the reference is right at once (" + std::to_string (resumed) + " dB after 0.5 s)");

            const float lowered = settle (tracker, -50.0, 90.0);
            check (lowered <= -45.0f, "a level 20 dB under the reference is activity, so the reference follows material that really got quieter (" + std::to_string (lowered) + " dB)");

            LevelTracker restarted;
            restarted.prepare (kRate, -50.0f);
            settle (restarted, -35.0, 60.0);
            restarted.restart();
            const float acquired = settle (restarted, -66.0, 2.0);
            check (std::abs (acquired - -66.0f) <= 1.5f, "after a restart material 31 dB under the old reference is acquired (" + std::to_string (acquired) + " dB)");
        }

        section ("Tracking 9: switching tracking on again discards the history from before tracking was off");
        {
            LevelTracker tracker;
            tracker.prepare (kRate, -40.0f);
            settle (tracker, -30.0, 60.0);
            tracker.setTracking (false);
            settle (tracker, -50.0, 60.0);
            tracker.setTracking (true);
            const float back = settle (tracker, -50.0, 1.0);
            check (std::abs (back - -50.0f) <= 2.0f, "one second after tracking resumes the reference is near the current level, not the one from before (" + std::to_string (back) + " dB)");
        }

        section ("Tracking 10: a jump to a region of the timeline that was played takes up what the tracker learned there");
        {
            const auto at = [] (double seconds) { return std::optional<std::int64_t> ((std::int64_t) (seconds * kRate)); };
            std::uint32_t state = 12345u;
            LevelTracker tracker;
            tracker.prepare (kRate, -20.0f);
            tracker.setTimeline (at (0.0));
            const float home = playIrregular (tracker, -20.0, 40.0, state);
            tracker.setTimeline (at (400.0));
            tracker.relocate();
            const float elsewhere = playIrregular (tracker, -32.0, 30.0, state);
            check (std::abs (elsewhere - home) > 8.0f, "the material at 400 s is far under the material at the start (" + std::to_string (elsewhere) + " against " + std::to_string (home) + " dB)");

            tracker.setTimeline (at (20.0));
            tracker.relocate();
            const float back = playIrregular (tracker, -20.0, 0.5, state);
            check (std::abs (back - home) <= 1.0f, "half a second after a return to a played region the reference is the one the region taught (" + std::to_string (back) + " against " + std::to_string (home) + " dB)");

            tracker.setTimeline (at (300.0));
            tracker.relocate();
            const float fresh = playIrregular (tracker, -30.0, 5.0, state);
            check (std::abs (fresh - -31.2f) <= 2.5f, "a jump more than 10 s from any played region restarts, and the reference follows the new level (" + std::to_string (fresh) + " dB)");

            LevelTracker unclocked;
            unclocked.prepare (kRate, -20.0f);
            playIrregular (unclocked, -20.0, 40.0, state);
            unclocked.relocate();
            const float restarted = playIrregular (unclocked, -32.0, 5.0, state);
            check (std::abs (restarted - -33.2f) <= 2.5f, "without a timeline a jump restarts (" + std::to_string (restarted) + " dB)");
        }
    }
}
