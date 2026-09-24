// Pushes synthetic audio through the real AirBandDSP class (no plugin
// hosting, no mocking of the signal path) and checks the OUTPUT WAVEFORM
// against expected physical behaviour. The point: a test that only checks
// "did it compile" or "did setParameters() throw" can pass while the
// signal path itself is silently broken (e.g. a stage wired to never run,
// output pinned to the dry signal, gain always collapsing to zero) -- the
// audio equivalent of every unit test passing while the game boots to a
// black screen. Only running real numbers through processBlock and
// measuring the result catches that class of bug.

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

#include <juce_dsp/juce_dsp.h>
#include "AirBandDSP.h"

namespace
{
    constexpr double kSampleRate = 44100.0;
    constexpr int kBlockSize = 512;

    int failureCount = 0;

    void check (bool condition, const std::string& description)
    {
        if (condition)
        {
            std::printf ("  [PASS] %s\n", description.c_str());
        }
        else
        {
            std::printf ("  [FAIL] %s\n", description.c_str());
            ++failureCount;
        }
    }

    double rmsDbOf (const std::vector<float>& samples, int startSample)
    {
        double sumSq = 0.0;
        int count = 0;

        for (size_t i = (size_t) startSample; i < samples.size(); ++i)
        {
            sumSq += (double) samples[i] * (double) samples[i];
            ++count;
        }

        const double rms = std::sqrt (sumSq / (double) juce::jmax (1, count));
        return 20.0 * std::log10 (juce::jmax (rms, 1.0e-12));
    }

    double peakOf (const std::vector<float>& samples, int startSample)
    {
        double peak = 0.0;
        for (size_t i = (size_t) startSample; i < samples.size(); ++i)
            peak = juce::jmax (peak, (double) std::abs (samples[i]));
        return peak;
    }

    double inputRmsDb (const std::function<float (int)>& generator, int numSamples, int startSample)
    {
        double sumSq = 0.0;
        int count = 0;

        for (int i = startSample; i < numSamples; ++i)
        {
            const double v = generator (i);
            sumSq += v * v;
            ++count;
        }

        const double rms = std::sqrt (sumSq / (double) juce::jmax (1, count));
        return 20.0 * std::log10 (juce::jmax (rms, 1.0e-12));
    }

    std::function<float (int)> sineWave (double freqHz, float amplitude)
    {
        return [freqHz, amplitude] (int n)
        {
            return amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * (double) n / kSampleRate);
        };
    }

    std::function<float (int)> multiTone (std::vector<double> freqsHz, float totalAmplitude)
    {
        const float perTone = totalAmplitude / (float) freqsHz.size();
        return [freqsHz, perTone] (int n)
        {
            float sum = 0.0f;
            for (double f : freqsHz)
                sum += perTone * (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * (double) n / kSampleRate);
            return sum;
        };
    }

    struct RunConfig
    {
        float midAirDb = 0.0f;
        float highAirDb = 0.0f;
        float blend = 0.2f;
        float outputDb = 0.0f;
        float deEss = 0.0f;
        float comp = 0.0f;
        float gate = 0.0f;
        float limiterCeilingDb = 0.0f;
    };

    // Runs `generator` (identical on both channels) through the real
    // AirBandDSP signal path in normal-sized host blocks and returns
    // channel 0 of the output, sample for sample. If latencyOut is
    // non-null, the DSP's reported latency (e.g. the limiter's lookahead)
    // is written to it, since the output is delayed by that many samples
    // relative to the input.
    std::vector<float> runThroughDSP (const std::function<float (int)>& generator, int numSamples,
                                       RunConfig cfg, bool& hadNonFinite, int* latencyOut = nullptr)
    {
        AirBandDSP dsp;
        dsp.prepare (kSampleRate, kBlockSize, 2);
        dsp.setParameters (cfg.midAirDb, cfg.highAirDb, cfg.blend, cfg.outputDb, cfg.deEss, cfg.comp, cfg.gate, cfg.limiterCeilingDb);

        if (latencyOut != nullptr)
            *latencyOut = dsp.getLatencySamples();

        std::vector<float> output;
        output.reserve ((size_t) numSamples);

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        hadNonFinite = false;

        int processed = 0;
        while (processed < numSamples)
        {
            const int thisBlock = juce::jmin (kBlockSize, numSamples - processed);
            buffer.setSize (2, thisBlock, false, false, true);

            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                for (int i = 0; i < thisBlock; ++i)
                    data[i] = generator (processed + i);
            }

            dsp.processBlock (buffer);

            auto* out0 = buffer.getReadPointer (0);
            for (int i = 0; i < thisBlock; ++i)
            {
                const float v = out0[i];
                if (! std::isfinite (v))
                    hadNonFinite = true;
                output.push_back (v);
            }

            processed += thisBlock;
        }

        return output;
    }
}

int main()
{
    const int twoSeconds = (int) (2.0 * kSampleRate);
    const int settleSample = (int) (1.0 * kSampleRate); // skip the first second so envelopes have settled

    std::printf ("AirBand DSP tests (%d samples/run @ %.0f Hz)\n\n", twoSeconds, kSampleRate);

    // --- Test 1: with every effect at its transparent setting, the plugin
    // must actually pass audio through unchanged (beyond the limiter's
    // reported lookahead delay) -- not silence it, not corrupt it. This is
    // the direct check against "tests pass, output is dead". The limiter
    // is always in the signal path, so its lookahead delay is real even
    // at a fully transparent ceiling; the comparison accounts for exactly
    // the delay the DSP itself reports, rather than special-casing it away. ---
    {
        std::printf ("Test 1: transparent settings pass audio through unchanged (beyond reported latency)\n");
        auto gen = sineWave (1000.0, 0.25f);
        bool nonFinite = false;
        int latency = 0;
        auto out = runThroughDSP (gen, twoSeconds, RunConfig {}, nonFinite, &latency);

        double maxAbsDiff = 0.0;
        for (int i = latency; i < twoSeconds; ++i)
            maxAbsDiff = juce::jmax (maxAbsDiff, (double) std::abs (out[(size_t) i] - gen (i - latency)));

        check (! nonFinite, "output contains no NaN/Inf");
        check (latency > 0, "DSP reports nonzero latency for the lookahead limiter (" + std::to_string (latency) + " samples)");
        check (maxAbsDiff < 1.0e-5, "output matches the delayed input exactly (max diff " + std::to_string (maxAbsDiff) + ")");
    }

    // --- Test 2: a quiet high-frequency tone must actually come out
    // louder with High Air engaged -- proves the boost law runs end to end. ---
    {
        std::printf ("Test 2: quiet high-frequency content is boosted by High Air\n");
        auto gen = sineWave (10000.0, 0.01f); // roughly -43 dBFS RMS
        RunConfig cfg;
        cfg.highAirDb = 15.0f;
        cfg.blend = 1.0f;

        bool nonFinite = false;
        auto out = runThroughDSP (gen, twoSeconds, cfg, nonFinite);

        const double inDb = inputRmsDb (gen, twoSeconds, settleSample);
        const double outDb = rmsDbOf (out, settleSample);
        const double gainDb = outDb - inDb;

        check (! nonFinite, "output contains no NaN/Inf");
        check (gainDb >= 3.0, "quiet tone gained at least +3 dB (measured " + std::to_string (gainDb) + " dB)");
        check (gainDb <= 16.0, "gain did not exceed the +15 dB High Air ceiling by more than headroom (measured " + std::to_string (gainDb) + " dB)");
    }

    // --- Test 3: a loud high-frequency tone must stay close to unity --
    // proves the level-dependent knee actually collapses the boost, not
    // just applies it uniformly. ---
    {
        std::printf ("Test 3: loud high-frequency content is left close to unity\n");
        auto gen = sineWave (10000.0, 0.9f); // roughly -4 dBFS RMS, above the -6 dBFS knee
        RunConfig cfg;
        cfg.highAirDb = 15.0f;
        cfg.blend = 1.0f;

        bool nonFinite = false;
        auto out = runThroughDSP (gen, twoSeconds, cfg, nonFinite);

        const double inDb = inputRmsDb (gen, twoSeconds, settleSample);
        const double outDb = rmsDbOf (out, settleSample);
        const double gainDb = outDb - inDb;

        check (! nonFinite, "output contains no NaN/Inf");
        check (std::abs (gainDb) <= 1.5, "loud tone stayed within 1.5 dB of unity (measured " + std::to_string (gainDb) + " dB)");
    }

    // --- Test 4: De-Ess must actually discriminate concentrated sibilant
    // energy from broadband high-frequency energy, not just apply a flat
    // reduction regardless of spectral shape. ---
    {
        std::printf ("Test 4: De-Ess suppresses concentrated sibilant energy more than broadband energy\n");
        auto sibilantGen = sineWave (6500.0, 0.05f); // dead centre of the sibilant sub-band
        auto broadbandGen = multiTone ({ 9500.0, 10500.0, 11500.0, 12500.0, 13500.0 }, 0.07f); // spread well clear of it

        RunConfig cfg;
        cfg.highAirDb = 15.0f;
        cfg.blend = 1.0f;
        cfg.deEss = 1.0f;

        bool nonFinite1 = false, nonFinite2 = false;
        auto sibilantOut = runThroughDSP (sibilantGen, twoSeconds, cfg, nonFinite1);
        auto broadbandOut = runThroughDSP (broadbandGen, twoSeconds, cfg, nonFinite2);

        const double sibilantGainDb = rmsDbOf (sibilantOut, settleSample) - inputRmsDb (sibilantGen, twoSeconds, settleSample);
        const double broadbandGainDb = rmsDbOf (broadbandOut, settleSample) - inputRmsDb (broadbandGen, twoSeconds, settleSample);

        check (! nonFinite1 && ! nonFinite2, "output contains no NaN/Inf");
        check (broadbandGainDb - sibilantGainDb >= 2.0,
               "sibilant content gained less than broadband content (sibilant " + std::to_string (sibilantGainDb)
                   + " dB, broadband " + std::to_string (broadbandGainDb) + " dB)");
    }

    // --- Test 5: the compressor must actually reduce gain on loud
    // content relative to quiet content, not just add a flat makeup gain
    // regardless of level. ---
    {
        std::printf ("Test 5: compressor reduces gain on loud content relative to quiet content\n");
        auto loudGen = sineWave (1000.0, 0.9f);   // well above the -18 dBFS threshold
        auto quietGen = sineWave (1000.0, 0.02f); // well below it

        RunConfig off, on;
        on.comp = 1.0f;

        bool nf1 = false, nf2 = false, nf3 = false, nf4 = false;
        auto loudOff = runThroughDSP (loudGen, twoSeconds, off, nf1);
        auto loudOn = runThroughDSP (loudGen, twoSeconds, on, nf2);
        auto quietOff = runThroughDSP (quietGen, twoSeconds, off, nf3);
        auto quietOn = runThroughDSP (quietGen, twoSeconds, on, nf4);

        const double deltaLoud = rmsDbOf (loudOn, settleSample) - rmsDbOf (loudOff, settleSample);
        const double deltaQuiet = rmsDbOf (quietOn, settleSample) - rmsDbOf (quietOff, settleSample);

        check (! nf1 && ! nf2 && ! nf3 && ! nf4, "output contains no NaN/Inf");
        check (std::abs (deltaQuiet - 6.0) <= 0.5,
               "quiet content (below threshold) gets the full +6 dB makeup gain (measured " + std::to_string (deltaQuiet) + " dB)");
        check (deltaQuiet - deltaLoud >= 2.0,
               "loud content gains measurably less than quiet content once compressed (quiet delta "
                   + std::to_string (deltaQuiet) + " dB, loud delta " + std::to_string (deltaLoud) + " dB)");
    }

    // --- Test 6: the gate must attenuate quiet content (breath/room
    // noise) while leaving normal-level content alone, not apply a flat
    // reduction regardless of level. ---
    {
        std::printf ("Test 6: gate attenuates quiet content but leaves loud content alone\n");
        auto quietGen = sineWave (1000.0, 0.005f); // well below the -40 dBFS threshold
        auto loudGen = sineWave (1000.0, 0.5f);    // well above it

        RunConfig off, on;
        on.gate = 1.0f;

        bool nf1 = false, nf2 = false, nf3 = false, nf4 = false;
        auto quietOff = runThroughDSP (quietGen, twoSeconds, off, nf1);
        auto quietOn = runThroughDSP (quietGen, twoSeconds, on, nf2);
        auto loudOff = runThroughDSP (loudGen, twoSeconds, off, nf3);
        auto loudOn = runThroughDSP (loudGen, twoSeconds, on, nf4);

        const double deltaQuiet = rmsDbOf (quietOn, settleSample) - rmsDbOf (quietOff, settleSample);
        const double deltaLoud = rmsDbOf (loudOn, settleSample) - rmsDbOf (loudOff, settleSample);

        check (! nf1 && ! nf2 && ! nf3 && ! nf4, "output contains no NaN/Inf");
        check (deltaQuiet <= -3.0, "quiet content (below threshold) is measurably attenuated (measured " + std::to_string (deltaQuiet) + " dB)");
        check (std::abs (deltaLoud) <= 0.5, "loud content (above threshold) is left alone (measured " + std::to_string (deltaLoud) + " dB)");
    }

    // --- Test 7: the limiter must actually keep the output peak at or
    // under its ceiling for a signal that would otherwise exceed it, not
    // just apply a flat gain regardless of the input peak. ---
    {
        std::printf ("Test 7: limiter keeps output peak at or under its ceiling\n");
        auto gen = sineWave (1000.0, 1.0f); // 0 dBFS peak
        RunConfig cfg;
        cfg.limiterCeilingDb = -6.0f;

        bool nonFinite = false;
        int latency = 0;
        auto out = runThroughDSP (gen, twoSeconds, cfg, nonFinite, &latency);

        const double ceilingLinear = juce::Decibels::decibelsToGain (cfg.limiterCeilingDb);
        const double outPeak = peakOf (out, settleSample);

        check (! nonFinite, "output contains no NaN/Inf");
        check (outPeak <= ceilingLinear * 1.05, "output peak stayed at or under the -6 dB ceiling (measured "
                                                     + std::to_string (outPeak) + ", ceiling " + std::to_string (ceilingLinear) + ")");
    }

    std::printf ("\n%s\n", failureCount == 0 ? "All tests passed." : "SOME TESTS FAILED.");
    return failureCount == 0 ? 0 : 1;
}
