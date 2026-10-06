#include <algorithm>
#include <cmath>
#include <string>

#include "AirBandDSP.h"
#include "Check.h"
#include "Harness.h"
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
    }
}
