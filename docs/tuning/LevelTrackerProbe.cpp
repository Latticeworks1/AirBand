// Runs the LevelTracker on the envelopes the plugin derives from a real recording, with the recording at its own
// level and scaled by plus and minus 12 dB, and compares the tracked reference with the true 90th percentile of the
// same envelope. The three signals are the ones that carry a reference: the 3 kHz and 9 kHz band envelopes of
// AirBand (2 ms, 0.3 ms and 150 ms detector) and the compressor's input (the largest magnitude of each 10 ms, which
// is what VocalCompressor hands the tracker).
//
// usage: LevelTrackerProbe in.wav [memorySeconds]
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "EnvelopeDetector.h"
#include "LevelTracker.h"
#include "VocalCompressor.h"

namespace
{
    std::vector<float> readMono (const char* path)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (path)));
        if (reader == nullptr) return {};
        juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
        return { buffer.getReadPointer (0), buffer.getReadPointer (0) + buffer.getNumSamples() };
    }

    // The compressor's tracker input: the largest magnitude since the start of the current 10 ms interval, so that the
    // value at the end of each interval (where the tracker reads it) is the interval's peak.
    std::vector<float> intervalPeak (const std::vector<float>& x, double rate)
    {
        const size_t interval = (size_t) std::lround (0.01 * rate);
        std::vector<float> env (x.size());
        float peak = 0.0f;
        for (size_t i = 0; i < x.size(); ++i)
        {
            if (i % interval == 0) peak = 0.0f;
            peak = std::max (peak, std::abs (x[i]));
            env[i] = peak;
        }
        return env;
    }

    std::vector<float> envelope (const std::vector<float>& x, double rate, double highpassHz, bool compressorInput)
    {
        if (compressorInput) return intervalPeak (x, rate);
        juce::dsp::IIR::Filter<float> hp;
        if (highpassHz > 0.0)
            hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (rate, (float) highpassHz, 0.707f);
        EnvelopeDetector detector;
        detector.prepare (rate, 0.002f, 0.0003f, 0.15f);
        std::vector<float> env (x.size());
        for (size_t i = 0; i < x.size(); ++i)
            env[i] = detector.pushSample (std::abs (highpassHz > 0.0 ? hp.processSample (x[i]) : x[i]));
        return env;
    }

    float toDb (float v) { return 20.0f * std::log10 (std::max (v, 1.0e-9f)); }

    // Of the values the tracker reads: one per 10 ms, at the end of each interval.
    float percentileDb (const std::vector<float>& env, double p)
    {
        std::vector<float> v;
        for (size_t i = 440; i < env.size(); i += 441) v.push_back (toDb (env[i]));
        std::sort (v.begin(), v.end());
        return v[(size_t) ((double) (v.size() - 1) * p / 100.0)];
    }
}

int main (int argc, char** argv)
{
    if (argc < 2) return 1;
    const auto x = readMono (argv[1]);
    if (x.empty()) return 1;
    const double rate = 44100.0;
    const float memory = argc > 2 ? (float) std::atof (argv[2]) : LevelTracker::kMemorySeconds;

    struct Source { const char* name; double highpassHz; bool peakInput; float design; };
    const Source sources[] = { { "3 kHz band", 3000.0, false, -31.0f }, { "9 kHz band", 9000.0, false, -43.0f }, { "compressor input", 0.0, true, VocalCompressor::kDesignReferenceDb } };

    std::printf ("%-20s %7s | true P90 | tracked reference (dB) at 5, 10, 20, 40, 80 and 155 s | mean absolute error after 20 s | range of the reference after 20 s\n", "signal", "scale");
    for (const auto& source : sources)
    {
        const auto own = envelope (x, rate, source.highpassHz, source.peakInput);
        std::printf ("%-20s level percentiles 5/25/50/75/90/95/99: %.1f %.1f %.1f %.1f %.1f %.1f %.1f dBFS\n", source.name,
                     (double) percentileDb (own, 5.0), (double) percentileDb (own, 25.0), (double) percentileDb (own, 50.0), (double) percentileDb (own, 75.0),
                     (double) percentileDb (own, 90.0), (double) percentileDb (own, 95.0), (double) percentileDb (own, 99.0));

        for (double scaleDb : { -12.0, 0.0, 12.0 })
        {
            std::vector<float> scaled (x);
            const float g = (float) std::pow (10.0, scaleDb / 20.0);
            for (auto& v : scaled) v *= g;
            const auto env = envelope (scaled, rate, source.highpassHz, source.peakInput);
            const float truth = percentileDb (env, 90.0);

            LevelTracker tracker;
            tracker.prepare (rate, source.design, memory);
            std::printf ("%-20s %+5.0f dB | %7.1f  |", source.name, scaleDb, (double) truth);
            double error = 0.0, lo = 1.0e9, hi = -1.0e9;
            size_t count = 0;
            const double marks[] = { 5, 10, 20, 40, 80, 155 };
            size_t next = 0;
            for (size_t i = 0; i < env.size(); ++i)
            {
                tracker.push (env[i]);
                const double t = (double) i / rate;
                if (next < 6 && t >= marks[next]) { std::printf (" %6.1f", (double) tracker.referenceDb()); ++next; }
                if (t >= 20.0)
                {
                    error += std::abs ((double) tracker.referenceDb() - (double) truth);
                    lo = std::min (lo, (double) tracker.referenceDb());
                    hi = std::max (hi, (double) tracker.referenceDb());
                    ++count;
                }
            }
            std::printf (" | %5.2f | %6.1f to %6.1f\n", error / (double) std::max<size_t> (1, count), lo, hi);
        }
    }
    return 0;
}
