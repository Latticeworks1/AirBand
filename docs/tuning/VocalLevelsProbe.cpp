// Distribution of the levels each AirBand stage sees on a real recording, using the production filters and
// envelope detector. Usage: VocalLevelsProbe <wav file>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "EnvelopeDetector.h"

namespace
{
    std::vector<float> readMono (const char* path)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (path)));
        if (reader == nullptr)
            return {};
        juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
        return { buffer.getReadPointer (0), buffer.getReadPointer (0) + buffer.getNumSamples() };
    }

    std::vector<float> envelopeDb (const std::vector<float>& x, double rate, double highpassHz)
    {
        juce::dsp::IIR::Filter<float> hp;
        if (highpassHz > 0.0)
            hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (rate, (float) highpassHz, 0.707f);
        EnvelopeDetector detector;
        detector.prepare (rate, 0.002f, 0.0003f, 0.15f); // as AirBand::prepare
        std::vector<float> db (x.size());
        for (size_t i = 0; i < x.size(); ++i)
        {
            const float band = highpassHz > 0.0 ? hp.processSample (x[i]) : x[i];
            db[i] = juce::Decibels::gainToDecibels (detector.pushSample (std::abs (band)), -120.0f);
        }
        return db;
    }

    float percentile (std::vector<float> v, double p)
    {
        std::sort (v.begin(), v.end());
        return v[(size_t) ((double) (v.size() - 1) * p / 100.0)];
    }
}

int main (int argc, char** argv)
{
    if (argc < 2) return 1;
    const auto x = readMono (argv[1]);
    if (x.empty()) { std::printf ("could not read %s\n", argv[1]); return 1; }
    const double rate = 44100.0;

    const auto broad = envelopeDb (x, rate, 0.0), mid = envelopeDb (x, rate, 3000.0), high = envelopeDb (x, rate, 9000.0);

    std::printf ("samples %zu (%.1f s)\n", x.size(), (double) x.size() / rate);
    std::printf ("\nEnvelope level in dBFS (AirBand's own detector), percentiles 5/25/50/75/90/95/99 and the largest value\n");
    for (const auto& [name, env] : { std::pair<const char*, const std::vector<float>*> { "broadband", &broad }, { "3 kHz high-pass", &mid }, { "9 kHz high-pass", &high } })
    {
        std::printf ("%-16s", name);
        for (double p : { 5.0, 25.0, 50.0, 75.0, 90.0, 95.0, 99.0, 100.0 })
            std::printf (" %7.1f", percentile (*env, p));
        std::printf ("\n");
    }

    // Samples where the broadband envelope is above the recording's own silence: the same distributions restricted to them.
    std::vector<float> midActive, highActive;
    for (size_t i = 0; i < x.size(); ++i)
        if (broad[i] > -50.0f)
        {
            midActive.push_back (mid[i]);
            highActive.push_back (high[i]);
        }
    std::printf ("\nSamples whose broadband envelope exceeds -50 dBFS: %.1f%% of the recording\n", 100.0 * (double) midActive.size() / (double) x.size());
    for (const auto& [name, env] : { std::pair<const char*, const std::vector<float>*> { "3 kHz high-pass", &midActive }, { "9 kHz high-pass", &highActive } })
    {
        std::printf ("%-16s", name);
        for (double p : { 5.0, 25.0, 50.0, 75.0, 90.0, 95.0, 99.0, 100.0 })
            std::printf (" %7.1f", percentile (*env, p));
        std::printf ("\n");
    }

    // The air law's knee: fraction of the maximum boost applied at a given band level for a collapse threshold T,
    // exactly the (1 - env/T)^2 of AirBand::processSample. Reported over the active samples.
    std::printf ("\nFraction of the maximum boost applied (knee^2) over the active samples, by collapse threshold T\n");
    std::printf ("band  T dBFS | mean   P10   P50   P90 | spread (P10 minus P90)\n");
    for (const auto& [name, env] : { std::pair<const char*, const std::vector<float>*> { "mid ", &midActive }, { "high", &highActive } })
        for (float threshold : { -6.0f, -14.0f, -20.0f, -26.0f, -32.0f, -38.0f, -44.0f })
        {
            std::vector<float> knee2 (env->size());
            double sum = 0.0;
            for (size_t i = 0; i < env->size(); ++i)
            {
                const float norm = juce::jlimit (0.0f, 1.0f, juce::Decibels::decibelsToGain ((*env)[i]) / juce::Decibels::decibelsToGain (threshold));
                knee2[i] = (1.0f - norm) * (1.0f - norm);
                sum += (double) knee2[i];
            }
            const float p10 = percentile (knee2, 10.0), p50 = percentile (knee2, 50.0), p90 = percentile (knee2, 90.0);
            std::printf ("%s  %6.0f | %5.2f %5.2f %5.2f %5.2f | %5.2f\n", name, (double) threshold, sum / (double) knee2.size(), (double) p10, (double) p50, (double) p90, (double) (p10 - p90));
        }

    std::printf ("\nGate and compressor: fraction of samples (all, then active) whose broadband envelope is below / above a level\n");
    for (float level : { -40.0f, -45.0f, -50.0f, -60.0f })
    {
        double all = 0.0, active = 0.0, activeCount = 0.0;
        for (float v : broad) all += v < level;
        for (float v : broad) if (v > -50.0f) { ++activeCount; active += v < level; }
        std::printf ("below %6.0f dBFS: %5.1f%% of all, %5.1f%% of active\n", (double) level, 100.0 * all / (double) broad.size(), activeCount > 0 ? 100.0 * active / activeCount : 0.0);
    }
    for (float level : { -18.0f, -12.0f, -6.0f })
    {
        double all = 0.0;
        for (float v : broad) all += v > level;
        std::printf ("above %6.0f dBFS: %5.1f%% of all\n", (double) level, 100.0 * all / (double) broad.size());
    }
    return 0;
}
