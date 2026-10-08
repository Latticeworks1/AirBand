// Renders a mono WAV through the production DSP with the given settings, aligned to the input by the reported
// latency, level matched to a reference RMS, and measures how much high-frequency level each stage adds frame by
// frame. Built once per source tree (AirBand sources are compiled in, not linked from a library) so that the
// released code and each variant of a constant can be compared on identical paths.
//
// usage: VocalRenderProbe in.wav out.wav|- midAirDb highAirDb blend deEss comp gate gateThresholdDb startSec durSec [refRmsDb, or 0 for none] [inputGainDb] [levelTracking 0|1]
//
// inputGainDb scales the recording before it enters the DSP, to compare how the stages treat the same material tracked at a
// different level; the reports are made against the scaled recording. Build with -DAIRBAND_HAS_GATE_THRESHOLD and
// -DAIRBAND_HAS_LEVEL_TRACKING for source trees that have those settings.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "AirBandDSP.h"

namespace
{
    std::vector<float> readMono (const char* path, double& rate)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (path)));
        if (reader == nullptr) return {};
        rate = reader->sampleRate;
        juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
        return { buffer.getReadPointer (0), buffer.getReadPointer (0) + buffer.getNumSamples() };
    }

    double rmsDb (const std::vector<float>& x)
    {
        double sum = 0.0;
        for (float v : x) sum += (double) v * v;
        return 10.0 * std::log10 (std::max (sum / (double) std::max<size_t> (1, x.size()), 1.0e-20));
    }

    std::vector<float> highpassed (const std::vector<float>& x, double rate, float hz)
    {
        juce::dsp::IIR::Filter<float> a, b;
        a.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (rate, hz, 0.707f);
        b.coefficients = a.coefficients;
        std::vector<float> y (x.size());
        for (size_t i = 0; i < x.size(); ++i) y[i] = b.processSample (a.processSample (x[i]));
        return y;
    }

    double percentile (std::vector<double> v, double p)
    {
        if (v.empty()) return 0.0;
        std::sort (v.begin(), v.end());
        return v[(size_t) ((double) (v.size() - 1) * p / 100.0)];
    }

    // Per-frame (50 ms) change of a high-passed band between the processed and the unprocessed excerpt, over the frames whose
    // unprocessed broadband RMS exceeds -45 dBFS, reported for the quietest, middle and loudest third of those frames ranked
    // by the unprocessed band's own level. A level-dependent boost shows as a larger lift in the quiet third.
    void reportLift (const char* name, const std::vector<float>& dry, const std::vector<float>& wet, double rate, float hz)
    {
        const auto d = highpassed (dry, rate, hz), w = highpassed (wet, rate, hz);
        const size_t frame = (size_t) (0.05 * rate);
        std::vector<std::pair<double, double>> frames; // (dry band level, lift)
        for (size_t s = 0; s + frame <= dry.size(); s += frame)
        {
            const std::vector<float> broad (dry.begin() + (long) s, dry.begin() + (long) (s + frame));
            if (rmsDb (broad) < -45.0) continue;
            const std::vector<float> db (d.begin() + (long) s, d.begin() + (long) (s + frame)), wb (w.begin() + (long) s, w.begin() + (long) (s + frame));
            frames.emplace_back (rmsDb (db), rmsDb (wb) - rmsDb (db));
        }
        std::sort (frames.begin(), frames.end());
        const auto meanOf = [&] (size_t lo, size_t hi) { double t = 0.0; for (size_t i = lo; i < hi; ++i) t += frames[i].second; return t / (double) std::max<size_t> (1, hi - lo); };
        const size_t n = frames.size();
        std::printf ("  %s band lift (dB), %zu active frames ranked by the band's own level: quietest third %.2f  middle %.2f  loudest third %.2f  (quiet minus loud %.2f)\n",
                     name, n, meanOf (0, n / 3), meanOf (n / 3, 2 * n / 3), meanOf (2 * n / 3, n), meanOf (0, n / 3) - meanOf (2 * n / 3, n));
    }

    // Percentiles of the 50 ms frame levels (frames above -60 dBFS), for the effect of the compressor on the level distribution.
    void reportFrameLevels (const char* name, const std::vector<float>& x, double rate)
    {
        const size_t frame = (size_t) (0.05 * rate);
        std::vector<double> levels;
        for (size_t s = 0; s + frame <= x.size(); s += frame)
        {
            const double level = rmsDb (std::vector<float> (x.begin() + (long) s, x.begin() + (long) (s + frame)));
            if (level > -60.0) levels.push_back (level);
        }
        std::printf ("  %s frame levels (dBFS) P10/P50/P90/P99/max: %.1f %.1f %.1f %.1f %.1f (%zu frames)\n", name,
                     percentile (levels, 10.0), percentile (levels, 50.0), percentile (levels, 90.0), percentile (levels, 99.0), percentile (levels, 100.0), levels.size());
    }

    // Mean broadband level change per band of unprocessed frame level, for the gate.
    void reportByLevel (const std::vector<float>& dry, const std::vector<float>& wet, double rate)
    {
        const size_t frame = (size_t) (0.05 * rate);
        const double edges[] = { -90.0, -60.0, -50.0, -45.0, -40.0, -35.0, -25.0, 0.0 };
        double sum[7] = {}; int count[7] = {};
        for (size_t s = 0; s + frame <= dry.size(); s += frame)
        {
            const std::vector<float> d (dry.begin() + (long) s, dry.begin() + (long) (s + frame)), w (wet.begin() + (long) s, wet.begin() + (long) (s + frame));
            const double level = rmsDb (d);
            for (int k = 0; k < 7; ++k)
                if (level >= edges[k] && level < edges[k + 1]) { sum[k] += rmsDb (w) - level; ++count[k]; }
        }
        std::printf ("  level change by frame RMS (dBFS):");
        for (int k = 0; k < 7; ++k)
            if (count[k] > 0) std::printf ("  [%.0f,%.0f): %.2f dB (%d)", edges[k], edges[k + 1], sum[k] / count[k], count[k]);
        std::printf ("\n");
    }
}

int main (int argc, char** argv)
{
    if (argc < 12) return 1;
    double rate = 0.0;
    auto all = readMono (argv[1], rate);
    if (all.empty()) return 1;

    const size_t from = (size_t) (std::atof (argv[10]) * rate), length = (size_t) (std::atof (argv[11]) * rate);
    std::vector<float> dry (all.begin() + (long) from, all.begin() + (long) std::min (all.size(), from + length));

    if (argc > 13)
    {
        const float g = (float) std::pow (10.0, std::atof (argv[13]) / 20.0);
        for (auto& v : dry) v *= g;
    }

    AirBandSettings settings;
    settings.midAirDb = (float) std::atof (argv[3]);
    settings.highAirDb = (float) std::atof (argv[4]);
    settings.blend = (float) std::atof (argv[5]);
    settings.deEssAmount = (float) std::atof (argv[6]);
    settings.compAmount = (float) std::atof (argv[7]);
    settings.gateAmount = (float) std::atof (argv[8]);
#ifdef AIRBAND_HAS_GATE_THRESHOLD
    settings.gateThresholdDb = (float) std::atof (argv[9]);
#endif
#ifdef AIRBAND_HAS_LEVEL_TRACKING
    settings.levelTracking = argc > 14 ? std::atoi (argv[14]) != 0 : true;
#endif

    AirBandDSP dsp;
    dsp.prepare (rate, 512, 1);
    dsp.setParameters (settings);
    const int latency = dsp.getLatencySamples();

    std::vector<float> wet;
    juce::AudioBuffer<float> buffer (1, 512);
    for (size_t pos = 0; pos < dry.size() + (size_t) latency; pos += 512)
    {
        const int n = (int) std::min<size_t> (512, dry.size() + (size_t) latency - pos);
        buffer.setSize (1, n, false, false, true);
        for (int i = 0; i < n; ++i) buffer.setSample (0, i, pos + (size_t) i < dry.size() ? dry[pos + (size_t) i] : 0.0f);
        dsp.processBlock (buffer);
        wet.insert (wet.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + n);
    }
    wet.erase (wet.begin(), wet.begin() + latency);
    wet.resize (dry.size());

    const double rawRms = rmsDb (wet);
    const double reference = argc > 12 && std::atof (argv[12]) < 0.0 ? std::atof (argv[12]) : rawRms; // a non-negative value means no level matching
    const float matchGain = (float) std::pow (10.0, (reference - rawRms) / 20.0);
    for (auto& v : wet) v *= matchGain;

    float peak = 0.0f;
    for (float v : wet) peak = std::max (peak, std::abs (v));
    std::printf ("latency %d samples, RMS %.2f dB (raw), match gain %.2f dB, peak %.1f dBFS\n", latency, rawRms, 20.0 * std::log10 (matchGain), 20.0 * std::log10 (std::max (peak, 1.0e-9f)));
    reportFrameLevels ("input ", dry, rate);
    reportFrameLevels ("output", wet, rate);
    reportLift ("3 kHz", dry, wet, rate, 3000.0f);
    reportLift ("9 kHz", dry, wet, rate, 9000.0f);
    reportByLevel (dry, wet, rate);

    if (std::string (argv[2]) != "-")
    {
        juce::File out (argv[2]);
        out.deleteFile();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::FileOutputStream> stream (out.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (stream.get(), rate, 1, 24, {}, 0));
        if (writer != nullptr)
        {
            stream.release();
            juce::AudioBuffer<float> whole (1, (int) wet.size());
            whole.copyFrom (0, 0, wet.data(), (int) wet.size());
            writer->writeFromAudioSampleBuffer (whole, 0, whole.getNumSamples());
        }
    }
    return 0;
}
