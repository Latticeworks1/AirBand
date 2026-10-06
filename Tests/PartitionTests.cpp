#include <string>
#include <vector>

#include <juce_dsp/juce_dsp.h>

#include "AirBandDSP.h"
#include "Check.h"
#include "Harness.h"
#include "Suites.h"
#include "TestMetrics.h"
#include "TestSettings.h"

// The DSP keeps all state per sample and applies parameters only between blocks, so for constant
// parameters (and parameter changes on a common block boundary) output is the same for every way
// of cutting the input into blocks: (y1, s1) = F(x[0:k], s0), (y2, s2) = F(x[k:N], s1) concatenate
// to F(x[0:N], s0). The same means bit-identical, except for the Intel snap-to-zero below.
namespace tests
{
    namespace
    {
        constexpr double kRate = 48000.0;

        // On Intel CPUs JUCE zeroes oversampler filter states below 1e-8 at the end of every processing
        // call (JUCE_DSP_ENABLE_SNAP_TO_ZERO), so the block cuts decide when a decaying state is flushed.
        // That perturbs the output by about one snap threshold, never more. Everywhere else the snap is
        // a no-op and every partition is bit-identical. Ten thresholds is the allowed difference there.
        double partitionTolerance()
        {
            float probe = 1.0e-9f;
            juce::dsp::util::snapToZero (probe);
            return probe == 0.0f ? 1.0e-7 : 0.0;
        }

        struct Partition
        {
            const char* name;
            std::vector<int> blocks;
        };

        const std::vector<Partition>& partitions()
        {
            static const std::vector<Partition> all = {
                { "1", { 1 } },          { "64", { 64 } },           { "128", { 128 } },
                { "512", { 512 } },      { "1024", { 1024 } },
                { "511+1+512", { 511, 1, 512 } }, { "257+255+512", { 257, 255, 512 } },
            };
            return all;
        }

        Signal boundaryTransients()
        {
            auto signal = makeNoise (16384, 0.05f, 7u);
            for (int at : { 511, 512, 1023, 1024, 4096 })
                signal[(size_t) at] = 0.9f;
            return signal;
        }

        void compare (const char* label, const Signal& input, const AirBandSettings& settings,
                      std::vector<ParameterStep> steps = {})
        {
            RenderOptions reference;
            reference.sampleRate = kRate;
            reference.steps = steps;
            const auto expected = render (input, settings, reference).output;

            for (const auto& partition : partitions())
            {
                RenderOptions options = reference;
                options.partition = partition.blocks;
                const auto actual = render (input, settings, options).output;
                const double worst = maxAbsDifference (actual, expected);
                check (worst <= partitionTolerance(),
                       std::string (label) + ", blocks " + partition.name + ": matches the one-block-size reference ("
                           + std::to_string (countDifferent (actual, expected)) + " samples differ, largest difference "
                           + scientific (worst) + ")");
            }
        }
    }

    void runPartitionTests()
    {
        section ("Partition 1: noise with every stage active is independent of block partitioning");
        compare ("noise", makeNoise (48000, 0.3f, 1u), featureSettings());

        section ("Partition 2: transients on block boundaries are independent of block partitioning");
        compare ("boundary transients", boundaryTransients(), featureSettings());

        section ("Partition 3: a parameter step on a shared block boundary is independent of block partitioning");
        compare ("parameter step", makeNoise (32768, 0.3f, 3u), featureSettings(), { { 16384, maximumSettings() } });

        section ("Partition 4: parameters take effect on block boundaries only");
        {
            // A step requested mid-block waits for the next block start; automation is not sample-accurate.
            const auto input = makeNoise (32768, 0.3f, 3u);
            const auto stepAt = [&] (int sample)
            {
                RenderOptions options;
                options.sampleRate = kRate;
                options.steps = { { sample, maximumSettings() } };
                return render (input, featureSettings(), options).output;
            };

            check (countDifferent (stepAt (16400), stepAt (16896)) == 0, "a step requested at 16400 lands at the next 512-sample block start, 16896");
            check (countDifferent (stepAt (16400), stepAt (16384)) > 0, "a step at the preceding block start 16384 sounds different");
        }

        section ("Partition 5: reset restores the power-on state");
        {
            AirBandDSP dsp;
            dsp.prepare (kRate, 512, 2);
            dsp.setParameters (featureSettings());
            const auto input = makeNoise (4096, 0.3f, 5u);

            const auto run = [&]
            {
                Signal out;
                juce::AudioBuffer<float> buffer (2, 512);
                for (size_t pos = 0; pos < input.size(); pos += 512)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        std::copy_n (input.data() + pos, 512, buffer.getWritePointer (ch));
                    dsp.processBlock (buffer);
                    out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 512);
                }
                return out;
            };

            const auto first = run();
            run();
            dsp.reset();
            check (countDifferent (run(), first) == 0, "output after reset matches the first run bit for bit");
        }
    }
}
