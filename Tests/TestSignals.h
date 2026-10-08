#pragma once

#include <vector>

// Deterministic stimulus generation. No assertions, no DSP.
namespace tests
{
    using Signal = std::vector<float>;

    constexpr double kDefaultRate = 44100.0;

    Signal makeSilence (int length);
    Signal makeImpulse (int length, int position, float amplitude);
    Signal makeSine (double freqHz, float amplitude, int length, double sampleRate = kDefaultRate);
    Signal makeTwoTone (double freqA, double freqB, float totalAmplitude, int length, double sampleRate = kDefaultRate);
    Signal makeMultiTone (const std::vector<double>& freqsHz, float totalAmplitude, int length,
                          double sampleRate = kDefaultRate);

    // Uniform noise in [-amplitude, amplitude) from a fixed xorshift32 stream, so the samples are
    // identical on every compiler and standard library.
    Signal makeNoise (int length, float amplitude, unsigned seed);

    // Uniform noise in bursts of burstLength samples, each burst with a peak amplitude drawn uniformly in dB between
    // lowDb and highDb (dBFS) from a fixed xorshift32 stream: program-like material whose level varies by burst.
    Signal makeLevelBursts (int length, int burstLength, double lowDb, double highDb, unsigned seed);

    // Each segment is a sine of the given amplitude, back to back.
    struct Segment
    {
        float amplitude;
        int length;
    };
    Signal makeAmplitudeSteps (double freqHz, const std::vector<Segment>& segments, double sampleRate = kDefaultRate);

    // Silence, then a sine burst, then silence.
    Signal makeBurst (int silenceBefore, int burstLength, int silenceAfter, double freqHz, float amplitude,
                      double sampleRate = kDefaultRate);
}
