#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "AirBandDSP.h"
#include "Check.h"
#include "Harness.h"
#include "Sanitize.h"
#include "Suites.h"
#include "TestMetrics.h"
#include "TestSettings.h"

namespace tests
{
    namespace
    {
        // Chunked processing must equal block-at-a-time processing exactly (see PartitionTests).
        void oversizedBlock()
        {
            section ("Safety 2: a block larger than the prepared size is processed without overrun");
            const auto input = makeNoise (8192, 0.3f, 9u);
            const auto expected = render (input, featureSettings()).output;

            RenderOptions options;
            options.maxBlockSize = 512;
            options.partition = { 8192 };
            const auto actual = render (input, featureSettings(), options).output;
            check (countDifferent (actual, expected) == 0, "one 8192-sample block into a 512-prepared DSP equals the 512-block result");

            // 1300 is not a multiple of 512: the chunks are 512, 512, 276, which is also a legal partition.
            options.partition = { 1300 };
            RenderOptions legal;
            legal.partition = { 512, 512, 276 };
            check (countDifferent (render (input, featureSettings(), options).output, render (input, featureSettings(), legal).output) == 0,
                   "1300-sample blocks equal the legal 512+512+276 partition of the same samples");
        }

        void channelTopology()
        {
            section ("Safety 3: channel counts: mono and stereo process, extra channels pass through");
            const auto input = makeNoise (4096, 0.3f, 11u);

            RenderOptions mono;
            mono.channels = 1;
            const auto monoOut = render (input, featureSettings(), mono).output;
            const auto stereoOut = render (input, featureSettings()).output;
            check (maxAbsDifference (monoOut, stereoOut) < 1.0e-5, "mono output matches the stereo output for identical channels (max diff "
                                                                        + std::to_string (maxAbsDifference (monoOut, stereoOut)) + ")");

            AirBandDSP dsp;
            dsp.prepare (48000.0, 512, 2);
            dsp.setParameters (featureSettings());
            juce::AudioBuffer<float> buffer (3, 256);
            for (int ch = 0; ch < 3; ++ch)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample (ch, i, 0.25f);
            dsp.processBlock (buffer);
            bool untouched = true;
            for (int i = 0; i < 256; ++i)
                untouched = untouched && buffer.getSample (2, i) == 0.25f;
            check (untouched, "a third channel beyond the prepared two is left unchanged");
        }

        void extremeInput()
        {
            section ("Safety 1: extreme input and settings stay finite and bounded");
            auto input = makeNoise (48000, 1.0f, 13u);
            for (size_t i = 10000; i < 10100; ++i)
                input[i] = 100.0f;

            for (const auto& settings : { minimumSettings(), maximumSettings(), featureSettings() })
            {
                RenderOptions options;
                options.sampleRate = 48000.0;
                const auto output = render (input, settings, options).output;
                check (allFinite (output), "output is finite");
            }

            auto limited = maximumSettings();
            limited.outputDb = 0.0f;
            limited.limiterCeilingDb = -6.0f;
            const auto output = render (input, limited).output;
            check (peak (output) <= std::pow (10.0, -6.0 / 20.0) * 1.05, "limiter holds a -6 dB ceiling against +40 dBFS input (peak "
                                                                              + std::to_string (peak (output)) + ")");
        }

        void nonFiniteInput()
        {
            section ("Safety 5: NaN and infinite input samples are treated as silence");
            const auto input = makeNoise (48000, 0.2f, 17u);
            const auto expected = render (input, featureSettings()).output;

            for (const float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity() })
            {
                auto poisoned = input;
                auto silenced = input;
                poisoned[10000] = bad;
                silenced[10000] = 0.0f;
                const auto actual = render (poisoned, featureSettings()).output;
                check (allFinite (actual), "output stays finite after a " + std::to_string (bad) + " sample");
                check (countDifferent (actual, render (silenced, featureSettings()).output) == 0,
                       "a " + std::to_string (bad) + " sample gives the same output as a zero sample");
            }
            for (const float big : { 1.0e30f, -3.0e38f })
            {
                auto spiked = input;
                spiked[10000] = big;
                check (allFinite (render (spiked, featureSettings()).output), "output stays finite after a " + std::to_string (big) + " sample");

                auto clamped = input;
                clamped[10000] = big > 0.0f ? kMaxInputMagnitude : -kMaxInputMagnitude;
                check (countDifferent (render (spiked, featureSettings()).output, render (clamped, featureSettings()).output) == 0,
                       "a " + std::to_string (big) + " sample is limited to +-" + std::to_string (kMaxInputMagnitude) + " (80 dBFS)");
            }
            check (countDifferent (expected, render (input, featureSettings()).output) == 0, "finite input is unaffected");
        }

        // A block-boundary change of the output gain must glide, not step. A constant input has no
        // slope of its own, so any single-sample jump in the output is the gain change itself.
        void gainGlide()
        {
            section ("Safety 6: output gain and blend changes glide instead of stepping");
            const int stepAt = 4 * 512;
            const Signal input ((size_t) (8 * 512), 0.1f);
            auto louder = featureSettings();
            louder.outputDb += 10.0f;
            louder.blend = 1.0f;

            RenderOptions options;
            options.steps = { { stepAt, louder } };
            const auto stepped = render (input, featureSettings(), options).output;

            // The limiter delays the change by its lookahead; the glide then lasts 20 ms.
            const size_t from = (size_t) stepAt, to = from + 1500;
            double largestJump = 0.0;
            for (size_t i = from; i < to; ++i)
                largestJump = std::max (largestJump, (double) std::abs (stepped[i] - stepped[i - 1]));
            const double change = (double) std::abs (stepped[to - 1] - stepped[from + 50]);

            check (change > 0.1, "the output level moved by " + std::to_string (change) + " across the +10 dB change");
            check (largestJump < 0.05 * change, "largest single-sample jump is " + std::to_string (largestJump / change) + " of the total change");
        }

        void degenerateCalls()
        {
            section ("Safety 4: degenerate calls are harmless");
            AirBandDSP unprepared;
            juce::AudioBuffer<float> buffer (2, 64);
            buffer.clear();
            unprepared.processBlock (buffer);
            check (true, "processBlock before prepare returns");

            AirBandDSP dsp;
            dsp.prepare (48000.0, 512, 2);
            juce::AudioBuffer<float> empty (2, 0);
            dsp.processBlock (empty);
            check (true, "zero-length block returns");
        }
    }

    void runSafetyTests()
    {
        extremeInput();
        oversizedBlock();
        channelTopology();
        degenerateCalls();
        nonFiniteInput();
        gainGlide();
    }
}
