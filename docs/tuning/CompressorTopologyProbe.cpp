// Three ways to build the compressor's level detection and gain smoothing, run on the same recording and on tones.
//
//   linear follower      the compressor of commit 0bd7078: the rectified input is smoothed in the linear domain (5 ms attack,
//                        100 ms release), converted to dB and passed through the gain computer; the gain is not smoothed again
//   peak, branching      the instantaneous magnitude in dB goes through the gain computer, and the required reduction is smoothed
//                        in dB with one attack and one release coefficient chosen by whether it is rising or falling
//   peak, decoupled      the same required reduction, held at its peaks and released from them with the release time constant,
//                        and then smoothed with the attack time constant (the topology of VocalCompressor)
//
// The gain computer is VocalCompressor::gainReductionDb with a ratio of 4:1 and the 2.25 dB makeup of full amount in every case.
// The threshold is 3 dB under the 90th percentile of the quantity the topology detects: -18.6 dBFS for the follower (threshold -21.6)
// and -14.3 dBFS for the 10 ms peaks (threshold -17.3); rows marked T=-19.3 move the peak threshold 2 dB lower.
//
// usage: CompressorTopologyProbe in.wav
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "EnvelopeDetector.h"
#include "VocalCompressor.h"

namespace {
std::vector<float> readMono (const char* path) {
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (path)));
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
    return { buffer.getReadPointer (0), buffer.getReadPointer (0) + buffer.getNumSamples() };
}
const double kRate = 44100.0;
float db (float v) { return 20.0f * std::log10 (std::max (v, 1.0e-6f)); }
float gain (float d) { return std::pow (10.0f, d / 20.0f); }

enum class Topology { follower, branching, decoupled };

struct Comp {
    Topology topology = Topology::follower;
    float ratio = 4.0f, thresholdDb = -21.6f, makeupDb = 2.25f, attack = 0.005f, release = 0.1f;
    EnvelopeDetector follower;
    float r = 0.0f, h = 0.0f;
    float aA = 0, aR = 0;
    void prepare() { follower.prepare (kRate, attack, attack, release); aA = std::exp (-1.0f / (attack * (float) kRate)); aR = std::exp (-1.0f / (release * (float) kRate)); r = h = 0; }
    float process (float x, float* grOut = nullptr) {
        float reduction;
        if (topology == Topology::follower) {
            reduction = VocalCompressor::gainReductionDb (db (follower.pushSample (std::abs (x))), thresholdDb, ratio);
        } else {
            const float target = VocalCompressor::gainReductionDb (db (std::abs (x)), thresholdDb, ratio);
            if (topology == Topology::branching) {
                r = target > r ? aA * r + (1 - aA) * target : aR * r + (1 - aR) * target;
            } else {
                h = std::max (target, aR * h + (1 - aR) * target);
                r = aA * r + (1 - aA) * h;
            }
            reduction = r;
        }
        if (grOut) *grOut = reduction;
        return x * gain (-reduction + makeupDb);
    }
};

double rmsDb (const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i]; return 10 * std::log10 (std::max (s / (double) (b - a), 1e-20)); }
}

int main (int argc, char** argv) {
    const auto x = readMono (argv[1]);
    // Level statistics of the candidates (tracking-independent).
    {
        EnvelopeDetector f; f.prepare (kRate, 0.005f, 0.005f, 0.1f);
        std::vector<float> follower, blockPeak, sample;
        float peak = 0; int c = 0;
        for (size_t i = 0; i < x.size(); ++i) {
            const float e = f.pushSample (std::abs (x[i]));
            if (i % 441 == 440) follower.push_back (db (e));
            peak = std::max (peak, std::abs (x[i]));
            if (++c == 441) { blockPeak.push_back (db (peak)); peak = 0; c = 0; }
            if (i % 441 == 220) sample.push_back (db (std::abs (x[i])));
        }
        const auto line = [] (const char* name, std::vector<float> v) {
            std::sort (v.begin(), v.end());
            const auto p = [&] (double q) { return (double) v[(size_t) ((double) (v.size() - 1) * q)]; };
            std::printf ("%-34s P5 %6.1f  P25 %6.1f  P50 %6.1f  P68 %6.1f  P75 %6.1f  P90 %6.1f  P95 %6.1f  P99 %6.1f  max %6.1f\n", name, p (.05), p (.25), p (.5), p (.68), p (.75), p (.9), p (.95), p (.99), p (1.0));
        };
        line ("follower 5/100 ms, at 10 ms", follower);
        line ("peak of each 10 ms", blockPeak);
        line ("instantaneous |x| at 10 ms (aliased)", sample);
    }

    struct Variant { const char* name; Topology topology; float threshold; float attack; float release; };
    const Variant variants[] = {
        { "linear follower (0bd7078)", Topology::follower, -21.6f, 0.005f, 0.1f },
        { "peak, branching 5/100 ms", Topology::branching, -17.3f, 0.005f, 0.1f },
        { "peak, decoupled 5/100 ms", Topology::decoupled, -17.3f, 0.005f, 0.1f },
        { "peak, branching 5/100, T=-19.3", Topology::branching, -19.3f, 0.005f, 0.1f },
        { "peak, decoupled 5/100, T=-19.3", Topology::decoupled, -19.3f, 0.005f, 0.1f },
        { "peak, decoupled 10/100, T=-17.3", Topology::decoupled, -17.3f, 0.010f, 0.1f },
        { "peak, decoupled 5/200, T=-17.3", Topology::decoupled, -17.3f, 0.005f, 0.2f },
    };

    // 1) the recording: 50 ms frame levels, applied reduction
    std::printf ("\nRecording at 100 percent (ratio 4:1, 6 dB knee, makeup 2.25 dB): 50 ms frame levels in dBFS and the applied gain reduction\n");
    std::printf ("%-34s | frame level P10  P50  P90  P99  max | P99-P50 | median change | reduction mean, P50, P90, P99, share >1 dB\n", "variant");
    const size_t frame = (size_t) (0.05 * kRate);
    const auto frames = [&] (const std::vector<float>& y) {
        std::vector<float> v; for (size_t a = 0; a + frame <= y.size(); a += frame) v.push_back ((float) rmsDb (y, a, a + frame));
        std::sort (v.begin(), v.end()); return v; };
    const auto inputFrames = frames (x);
    const auto q = [] (const std::vector<float>& v, double p) { return (double) v[(size_t) ((double) (v.size() - 1) * p)]; };
    std::printf ("%-34s | %6.1f %5.1f %5.1f %5.1f %5.1f | %5.1f\n", "input", q (inputFrames, .1), q (inputFrames, .5), q (inputFrames, .9), q (inputFrames, .99), q (inputFrames, 1.0), q (inputFrames, .99) - q (inputFrames, .5));
    for (const auto& v : variants) {
        Comp c; c.topology = v.topology; c.thresholdDb = v.threshold; c.attack = v.attack; c.release = v.release; c.prepare();
        std::vector<float> y (x.size()), gr (x.size());
        for (size_t i = 0; i < x.size(); ++i) { float g; y[i] = c.process (x[i], &g); gr[i] = g; }
        const auto f = frames (y);
        std::vector<float> g2 (gr); std::sort (g2.begin(), g2.end());
        double mean = 0; size_t over = 0; for (float g : gr) { mean += g; over += g > 1.0f; }
        std::printf ("%-34s | %6.1f %5.1f %5.1f %5.1f %5.1f | %5.1f | %+5.2f | %.2f %.2f %.2f %.2f %.1f%%\n", v.name, q (f, .1), q (f, .5), q (f, .9), q (f, .99), q (f, 1.0), q (f, .99) - q (f, .5), q (f, .5) - q (inputFrames, .5),
                     mean / (double) gr.size(), (double) g2[g2.size() / 2], (double) g2[(size_t) ((double) g2.size() * 0.9)], (double) g2[(size_t) ((double) g2.size() * 0.99)], 100.0 * (double) over / (double) gr.size());
    }

    // 2) harmonic distortion of low tones: ratio of the energy at 2..8 times the fundamental to the fundamental, dB
    std::printf ("\nHarmonic distortion (2nd to 8th harmonic relative to the fundamental, dB) of a steady tone at 100 percent; 1 s analysed after 2 s\n");
    std::printf ("%-34s |", "variant");
    struct Tone { double f; float levelDb; };
    const Tone tones[] = { { 60, -6 }, { 100, -6 }, { 100, -12 }, { 200, -6 }, { 200, -12 }, { 500, -6 }, { 1000, -6 } };
    for (const auto& t : tones) std::printf (" %4.0fHz/%+3.0f", t.f, (double) t.levelDb);
    std::printf ("\n");
    for (const auto& v : variants) {
        std::printf ("%-34s |", v.name);
        for (const auto& t : tones) {
            Comp c; c.topology = v.topology; c.thresholdDb = v.threshold; c.attack = v.attack; c.release = v.release; c.prepare();
            const size_t n = (size_t) (3 * kRate); std::vector<float> y (n);
            const float amp = gain (t.levelDb);
            for (size_t i = 0; i < n; ++i) y[i] = c.process (amp * (float) std::sin (2 * M_PI * t.f * (double) i / kRate));
            // single-bin DFT at harmonics over an integer number of cycles
            const size_t start = (size_t) (2 * kRate);
            const size_t cycles = (size_t) std::floor (t.f * 1.0); const size_t len = (size_t) std::llround ((double) cycles * kRate / t.f);
            const auto mag = [&] (double f) { std::complex<double> a = 0; for (size_t i = 0; i < len; ++i) a += (double) y[start + i] * std::polar (1.0, -2 * M_PI * f * (double) (start + i) / kRate); return std::abs (a) * 2 / (double) len; };
            const double fund = mag (t.f); double h = 0; for (int k = 2; k <= 8; ++k) h += std::pow (mag (k * t.f), 2);
            std::printf (" %10.1f", 10 * std::log10 (std::max (h, 1e-20) / (fund * fund)));
        }
        std::printf ("\n");
    }

    // 3) step response: 1 kHz at -30 dBFS for 1 s then -10 dBFS; reduction vs time after the step, and final value
    std::printf ("\nStep response of the reduction (dB) for 1 kHz stepping from -30 to -10 dBFS at t=0 and back at 1 s: reduction at 1, 2, 5, 10, 20, 50, 100, 300 ms; release: reduction 20, 50, 100, 200, 400, 800 ms after the step down\n");
    for (const auto& v : variants) {
        Comp c; c.topology = v.topology; c.thresholdDb = v.threshold; c.attack = v.attack; c.release = v.release; c.prepare();
        std::vector<float> g (size_t (2.5 * kRate));
        for (size_t i = 0; i < g.size(); ++i) {
            const double t = (double) i / kRate - 0.5;
            const float amp = gain (t < 0 ? -30.0f : t < 1.0 ? -10.0f : -30.0f);
            float r; c.process (amp * (float) std::sin (2 * M_PI * 1000.0 * (double) i / kRate), &r); g[i] = r;
        }
        const size_t s0 = (size_t) (0.5 * kRate);
        std::printf ("%-34s |", v.name);
        // report the peak-envelope (max over the following 1 ms) to avoid cycle ripple
        const auto env = [&] (size_t at) { float m = 0; for (size_t i = at; i < at + 44 && i < g.size(); ++i) m = std::max (m, g[i]); return (double) m; };
        for (double ms : { 1, 2, 5, 10, 20, 50, 100, 300 }) std::printf (" %5.2f", env (s0 + (size_t) (ms * 0.001 * kRate)));
        std::printf (" |");
        const size_t s1 = (size_t) (1.5 * kRate);
        for (double ms : { 20, 50, 100, 200, 400, 800 }) std::printf (" %5.2f", env (s1 + (size_t) (ms * 0.001 * kRate)));
        std::printf ("\n");
    }
    return 0;
}
