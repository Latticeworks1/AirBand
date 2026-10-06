#include "TestSignals.h"

#include <cmath>
#include <numbers>

namespace tests
{
    namespace
    {
        float sineAt (double freqHz, int n, double sampleRate)
        {
            return (float) std::sin (2.0 * std::numbers::pi * freqHz * (double) n / sampleRate);
        }
    }

    Signal makeSilence (int length)
    {
        return Signal ((size_t) length, 0.0f);
    }

    Signal makeImpulse (int length, int position, float amplitude)
    {
        auto signal = makeSilence (length);
        signal[(size_t) position] = amplitude;
        return signal;
    }

    Signal makeSine (double freqHz, float amplitude, int length, double sampleRate)
    {
        Signal signal ((size_t) length);
        for (int n = 0; n < length; ++n)
            signal[(size_t) n] = amplitude * sineAt (freqHz, n, sampleRate);
        return signal;
    }

    Signal makeMultiTone (const std::vector<double>& freqsHz, float totalAmplitude, int length, double sampleRate)
    {
        const float perTone = totalAmplitude / (float) freqsHz.size();
        Signal signal ((size_t) length);
        for (int n = 0; n < length; ++n)
        {
            float sum = 0.0f;
            for (double f : freqsHz)
                sum += perTone * sineAt (f, n, sampleRate);
            signal[(size_t) n] = sum;
        }
        return signal;
    }

    Signal makeTwoTone (double freqA, double freqB, float totalAmplitude, int length, double sampleRate)
    {
        return makeMultiTone ({ freqA, freqB }, totalAmplitude, length, sampleRate);
    }

    Signal makeNoise (int length, float amplitude, unsigned seed)
    {
        unsigned state = seed == 0 ? 1u : seed;
        Signal signal ((size_t) length);
        for (auto& sample : signal)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            sample = amplitude * ((float) (state >> 8) / 8388608.0f - 1.0f);
        }
        return signal;
    }

    Signal makeAmplitudeSteps (double freqHz, const std::vector<Segment>& segments, double sampleRate)
    {
        Signal signal;
        for (const auto& segment : segments)
            for (int i = 0; i < segment.length; ++i)
                signal.push_back (segment.amplitude * sineAt (freqHz, (int) signal.size(), sampleRate));
        return signal;
    }

    Signal makeBurst (int silenceBefore, int burstLength, int silenceAfter, double freqHz, float amplitude,
                      double sampleRate)
    {
        auto signal = makeSilence (silenceBefore);
        const auto burst = makeSine (freqHz, amplitude, burstLength, sampleRate);
        signal.insert (signal.end(), burst.begin(), burst.end());
        signal.resize (signal.size() + (size_t) silenceAfter, 0.0f);
        return signal;
    }
}
