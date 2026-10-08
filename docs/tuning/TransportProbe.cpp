// How the LevelTracker behaves when the transport jumps, when playback starts, when the signal pauses and when a loop
// comes round, on envelopes made from a real recording with AirBand's own detectors. Each scenario is a splice of 30 s
// segments of the recording (five of them, optionally scaled in level), the tracker is told what the plugin would tell it
// (relocate() at a jump, with the position on the timeline, and nothing at a loop wrap or when playback simply continues; the
// START, JUMP, GATE and PAUSE scenarios give no timeline, so that relocate() is restart()), and the tracked reference is compared
// with the true 90th percentile of the segment that is playing.
//
// usage: TransportProbe in.wav
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "EnvelopeDetector.h"
#include "LevelTracker.h"
#include "VocalCompressor.h"

namespace
{
    constexpr double kRate = 44100.0;

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

    // The tracker reads the value at the end of each 10 ms interval. For the compressor that is the interval's peak; for
    // a band it is the envelope of AirBand's detector.
    std::vector<float> intervalPeak (const std::vector<float>& x)
    {
        std::vector<float> env (x.size());
        float peak = 0.0f;
        for (size_t i = 0; i < x.size(); ++i)
        {
            if (i % 441 == 0) peak = 0.0f;
            peak = std::max (peak, std::abs (x[i]));
            env[i] = peak;
        }
        return env;
    }

    std::vector<float> bandEnvelope (const std::vector<float>& x, double highpassHz)
    {
        juce::dsp::IIR::Filter<float> hp;
        hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (kRate, (float) highpassHz, 0.707f);
        EnvelopeDetector detector;
        detector.prepare (kRate, 0.002f, 0.0003f, 0.15f);
        std::vector<float> env (x.size());
        for (size_t i = 0; i < x.size(); ++i)
            env[i] = detector.pushSample (std::abs (hp.processSample (x[i])));
        return env;
    }

    float toDb (float v) { return 20.0f * std::log10 (std::max (v, 1.0e-9f)); }

    float p90 (const std::vector<float>& env)
    {
        std::vector<float> v;
        for (size_t i = 440; i < env.size(); i += 441) v.push_back (toDb (env[i]));
        std::sort (v.begin(), v.end());
        return v[(size_t) ((double) (v.size() - 1) * 0.9)];
    }

    struct Source { const char* name; bool peak; double highpassHz; float design; };

    struct Variant { const char* name; bool restart; int acquire; float gate; };

    // Mean absolute error by time since the splice (0 to 0.5, 0.5 to 2, 2 to 5, 5 to 10, 10 to 20 and 20 to 30 s).
    void errors (const std::vector<float>& before, const std::vector<float>& after, const Variant& v, const Source& s, double* out)
    {
        LevelTracker tracker;
        tracker.prepare (kRate, s.design, LevelTracker::Options { LevelTracker::kMemorySeconds, v.gate, v.acquire });
        for (float e : before) tracker.push (e);
        if (v.restart) tracker.restart();
        const float truth = p90 (after);
        const double edges[] = { 0.5, 2.0, 5.0, 10.0, 20.0 };
        double sums[6] = {};
        size_t counts[6] = {};
        for (size_t i = 0; i < after.size(); ++i)
        {
            tracker.push (after[i]);
            int k = 0;
            while (k < 5 && (double) i / kRate >= edges[k]) ++k;
            sums[k] += std::abs ((double) tracker.referenceDb() - truth);
            ++counts[k];
        }
        for (int k = 0; k < 6; ++k) out[k] += sums[k] / (double) counts[k];
    }

    // 0: no action at the jumps, 1: restart(), 2: setTimeline() and relocate() as the plugin does. The tracker plays `a`
    // from the start of the timeline, jumps to `b` (never played), and jumps back to 10 s into `a`; the error is against the
    // 90th percentile of that rest of `a`, by time since the return (<0.5 <2 <5 <10 <20 s).
    void returnErrors (const std::vector<float>& a, const std::vector<float>& b, int mode, const Source& s, double* out)
    {
        LevelTracker tracker;
        tracker.prepare (kRate, s.design);
        const auto at = [] (double seconds) { return std::optional<std::int64_t> ((std::int64_t) (seconds * kRate)); };
        const auto jump = [&] (double seconds)
        {
            if (mode == 2) { tracker.setTimeline (at (seconds)); tracker.relocate(); }
            else if (mode == 1) tracker.restart();
        };
        if (mode == 2) tracker.setTimeline (at (0.0));
        for (float e : a) tracker.push (e);
        jump (400.0);
        for (float e : b) tracker.push (e);
        jump (10.0);
        const std::vector<float> after (a.begin() + (long) (10.0 * kRate), a.end());
        const float truth = p90 (after);
        const double edges[] = { 0.5, 2.0, 5.0, 10.0 };
        double sums[5] = {};
        size_t counts[5] = {};
        for (size_t i = 0; i < after.size(); ++i)
        {
            tracker.push (after[i]);
            int k = 0;
            while (k < 4 && (double) i / kRate >= edges[k]) ++k;
            sums[k] += std::abs ((double) tracker.referenceDb() - truth);
            ++counts[k];
        }
        for (int k = 0; k < 5; ++k) out[k] += sums[k] / (double) counts[k];
    }
}

int main (int argc, char** argv)
{
    if (argc < 2) return 1;
    const auto x = readMono (argv[1]);
    if (x.empty()) return 1;
    const size_t seg = (size_t) (30.0 * kRate);

    const Source sources[] = { { "compressor input (peak of each 10 ms)", true, 0.0, VocalCompressor::kDesignReferenceDb }, { "3 kHz band", false, 3000.0, -31.0f } };

    for (const auto& s : sources)
    {
        const auto base = s.peak ? intervalPeak (x) : bandEnvelope (x, s.highpassHz);
        const auto segment = [&] (int index, double scaleDb)
        {
            std::vector<float> v (base.begin() + (long) ((size_t) index * seg), base.begin() + (long) ((size_t) (index + 1) * seg));
            const float g = (float) std::pow (10.0, scaleDb / 20.0);
            for (auto& e : v) e *= g;
            return v;
        };

        std::printf ("\n=== %s ===\n", s.name);
        std::printf ("\nJUMP: 30 s of one segment, then the transport jumps to another (20 ordered pairs); mean absolute error (dB) of the reference against the new segment's own 90th percentile by time since the jump: <0.5 <2 <5 <10 <20 <30 s\n");
        const Variant jumps[] = { { "no restart", false, 30, 30.0f }, { "restart, acquire 20", true, 20, 30.0f }, { "restart, acquire 30 (plugin)", true, 30, 30.0f },
                                  { "restart, acquire 50", true, 50, 30.0f }, { "restart, acquire 100", true, 100, 30.0f } };
        for (double scaleDb : { -12.0, 0.0, 12.0 })
        {
            std::printf ("-- the new segment %+.0f dB\n", scaleDb);
            for (const auto& v : jumps)
            {
                double sum[6] = {};
                int pairs = 0;
                for (int a = 0; a < 5; ++a)
                    for (int b = 0; b < 5; ++b)
                        if (a != b) { errors (segment (a, 0.0), segment (b, scaleDb), v, s, sum); ++pairs; }
                std::printf ("   %-30s", v.name);
                for (double e : sum) std::printf (" %6.2f", e / pairs);
                std::printf ("\n");
            }
        }

        std::printf ("\nRETURN: 30 s of one segment, a jump to a segment that was never played, and a jump back to 10 s into the first (20 ordered pairs); mean absolute error (dB) against the 90th percentile of the rest of the first segment by time since the return: <0.5 <2 <5 <10 <20 s\n");
        for (double scaleDb : { -12.0, 0.0 })
        {
            std::printf ("-- the other segment %+.0f dB\n", scaleDb);
            const char* names[] = { "no restart", "restart", "region memory (plugin)" };
            for (int mode = 0; mode < 3; ++mode)
            {
                double sum[5] = {};
                int pairs = 0;
                for (int a = 0; a < 5; ++a)
                    for (int b = 0; b < 5; ++b)
                        if (a != b) { returnErrors (segment (a, 0.0), segment (b, scaleDb), mode, s, sum); ++pairs; }
                std::printf ("   %-30s", names[mode]);
                for (double e : sum) std::printf (" %6.2f", e / pairs);
                std::printf ("\n");
            }
        }

        std::printf ("\nSTART: playback begins on one segment; the reference starts at the design level; same columns\n");
        for (double scaleDb : { -12.0, 0.0, 12.0 })
        {
            double sum[6] = {};
            for (int b = 0; b < 5; ++b)
                errors ({}, segment (b, scaleDb), { "start", false, 30, 30.0f }, s, sum);
            std::printf ("   segment %+3.0f dB                 ", scaleDb);
            for (double e : sum) std::printf (" %6.2f", e / 5.0);
            std::printf ("\n");
        }

        std::printf ("\nGATE: mean offset (dB) of the tracked reference from the true 90th percentile of the whole recording, taken over the time after 20 s; the recording scaled by -12, 0 and +12 dB\n");
        for (const float gate : { 0.0f, 30.0f })
        {
            std::printf ("   relative gate %-8s", gate > 0.0f ? "30 dB" : "off");
            for (double scaleDb : { -12.0, 0.0, 12.0 })
            {
                LevelTracker tracker;
                tracker.prepare (kRate, s.design, LevelTracker::Options { LevelTracker::kMemorySeconds, gate, LevelTracker::kAcquireUpdates });
                const float g = (float) std::pow (10.0, scaleDb / 20.0);
                std::vector<float> scaled (base);
                for (auto& e : scaled) e *= g;
                const float truth = p90 (scaled);
                double offset = 0.0;
                size_t count = 0;
                for (size_t i = 0; i < scaled.size(); ++i)
                {
                    tracker.push (scaled[i]);
                    if ((double) i / kRate >= 20.0) { offset += (double) tracker.referenceDb() - truth; ++count; }
                }
                std::printf ("  %+6.2f", offset / (double) count);
            }
            std::printf ("\n");
        }

        std::printf ("\nPAUSE: 20 s of the recording, a pause of constant noise 40 dB under the design reference, then the recording again; reference at the end of the pause, and mean absolute error by time since resuming: <0.5 <2 <5 <20 s\n");
        {
            const auto program = segment (2, 0.0);
            const std::vector<float> head (program.begin(), program.begin() + (long) (20.0 * kRate));
            const float truth = p90 (program);
            const float noise = std::pow (10.0f, (s.design - 40.0f) / 20.0f);
            for (double pause : { 10.0, 30.0, 90.0 })
                for (const Variant v : { Variant { "relative gate off", false, 30, 0.0f }, Variant { "relative gate 30 dB (plugin)", false, 30, 30.0f } })
                {
                    LevelTracker tracker;
                    tracker.prepare (kRate, s.design, LevelTracker::Options { LevelTracker::kMemorySeconds, v.gate, v.acquire });
                    for (float e : head) tracker.push (e);
                    const float before = tracker.referenceDb();
                    for (size_t i = 0; i < (size_t) (pause * kRate); ++i) tracker.push (noise);
                    const float endOfPause = tracker.referenceDb();
                    double sums[4] = {};
                    size_t counts[4] = {};
                    const double edges[] = { 0.5, 2.0, 5.0 };
                    for (size_t i = 0; i < head.size(); ++i)
                    {
                        tracker.push (head[i]);
                        int k = 0;
                        while (k < 3 && (double) i / kRate >= edges[k]) ++k;
                        sums[k] += std::abs ((double) tracker.referenceDb() - truth);
                        ++counts[k];
                    }
                    std::printf ("   pause %3.0f s, %-28s before %6.1f, end of pause %6.1f; after: %5.2f %5.2f %5.2f %5.2f\n", pause, v.name, (double) before, (double) endOfPause,
                                 sums[0] / (double) counts[0], sums[1] / (double) counts[1], sums[2] / (double) counts[2], sums[3] / (double) counts[3]);
                }
        }

        std::printf("\nLOOP: a 16 s loop of the recording, six passes; mean absolute error (dB) of the reference against the loop's own 90th percentile by position within the pass, passes 2 to 6: <1 <4 <10 <16 s\n");
        for (int startSec : { 10, 60, 100 })
        {
            const size_t a = (size_t) (startSec * kRate), len = (size_t) (16.0 * kRate);
            const std::vector<float> loop (base.begin() + (long) a, base.begin() + (long) (a + len));
            const float truth = p90 (loop);
            for (const bool restart : { false, true })
            {
                LevelTracker tracker;
                tracker.prepare (kRate, s.design);
                double sums[4] = {};
                size_t counts[4] = {};
                for (int pass = 0; pass < 6; ++pass)
                {
                    if (pass > 0 && restart) tracker.restart();
                    for (size_t i = 0; i < len; ++i)
                    {
                        tracker.push (loop[i]);
                        if (pass == 0) continue;
                        const double t = (double) i / kRate;
                        const int k = t < 1 ? 0 : t < 4 ? 1 : t < 10 ? 2 : 3;
                        sums[k] += std::abs ((double) tracker.referenceDb() - truth);
                        ++counts[k];
                    }
                }
                std::printf ("   loop from %3d s, truth %6.1f, %-26s %5.2f %5.2f %5.2f %5.2f\n", startSec, (double) truth, restart ? "restart at each wrap" : "no restart (loop wrap)",
                             sums[0] / (double) counts[0], sums[1] / (double) counts[1], sums[2] / (double) counts[2], sums[3] / (double) counts[3]);
            }
        }
    }
    return 0;
}
