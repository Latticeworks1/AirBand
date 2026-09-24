#include "VocalLimiter.h"

void VocalLimiter::prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
{
    sr = sampleRate;
    lookaheadSamples = (int) std::round (0.005 * sr); // 5 ms
    ringSize = lookaheadSamples + 1;

    delayLines.assign ((size_t) numChannels, std::vector<float> ((size_t) ringSize, 0.0f));
    peakRing.assign ((size_t) ringSize, 0.0f);

    releaseCoeff = std::exp (-1.0f / (0.06f * (float) sr)); // 60 ms release

    reset();
}

void VocalLimiter::reset()
{
    for (auto& line : delayLines)
        std::fill (line.begin(), line.end(), 0.0f);

    std::fill (peakRing.begin(), peakRing.end(), 0.0f);

    writeIndex = 0;
    currentGain = 1.0f;
}

void VocalLimiter::setCeilingDb (float ceilingDb)
{
    ceilingLinear = juce::Decibels::decibelsToGain (ceilingDb);
}

void VocalLimiter::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) delayLines.size());
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float inputPeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float sample = buffer.getReadPointer (ch)[i];
            delayLines[(size_t) ch][(size_t) writeIndex] = sample;
            inputPeak = juce::jmax (inputPeak, std::abs (sample));
        }
        peakRing[(size_t) writeIndex] = inputPeak;

        // The sample about to be output sits `lookaheadSamples` behind the
        // one just written; scan forward from it to the newest sample to
        // find the true peak that is about to arrive.
        const int readIndex = (writeIndex - lookaheadSamples + ringSize) % ringSize;

        float windowPeak = 0.0f;
        for (int k = 0; k <= lookaheadSamples; ++k)
            windowPeak = juce::jmax (windowPeak, peakRing[(size_t) ((readIndex + k) % ringSize)]);

        const float requiredGain = ceilingLinear / juce::jmax (windowPeak, ceilingLinear);

        if (requiredGain < currentGain)
            currentGain = requiredGain; // instant attack: the peak must not exceed the ceiling
        else
            currentGain += (requiredGain - currentGain) * (1.0f - releaseCoeff);

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] = delayLines[(size_t) ch][(size_t) readIndex] * currentGain;

        writeIndex = (writeIndex + 1) % ringSize;
    }
}
