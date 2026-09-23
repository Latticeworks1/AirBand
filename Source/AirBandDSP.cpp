#include "AirBandDSP.h"

void AirBand::prepare (double sampleRate, int /*maxBlockSize*/, float highpassHz)
{
    sr = sampleRate;
    highpassFreq = highpassHz;

    highpassCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, highpassFreq, 0.707f);
    highpass.coefficients = highpassCoeffs;
    highpass.reset();

    // ~2 ms fast detector, ~0.3 ms peak catch, ~150 ms program-dependent
    // release. These land in the same range as the dual time-constant
    // schemes used in companders of this era; they are not swept/ablated
    // against a reference, just carried over from that convention.
    attackCoeffFast = std::exp (-1.0f / (0.002f * (float) sr));
    attackCoeffPeak = std::exp (-1.0f / (0.0003f * (float) sr));
    releaseCoeff = std::exp (-1.0f / (0.15f * (float) sr));

    reset();
}

void AirBand::reset()
{
    highpass.reset();
    envelopeFast = 0.0f;
    envelopePeak = 0.0f;
}

void AirBand::setParameters (float boostDb, float thresholdDb)
{
    boostLinearMax = juce::Decibels::decibelsToGain (boostDb);
    thresholdLinear = juce::Decibels::decibelsToGain (thresholdDb);
}

float AirBand::processSample (float x)
{
    const float band = highpass.processSample (x);
    const float rectified = std::abs (band);

    // Fast level tracker.
    if (rectified > envelopeFast)
        envelopeFast = attackCoeffFast * envelopeFast + (1.0f - attackCoeffFast) * rectified;
    else
        envelopeFast = releaseCoeff * envelopeFast + (1.0f - releaseCoeff) * rectified;

    // Faster peak catch so a transient can't outrun the detector.
    if (rectified > envelopePeak)
        envelopePeak = attackCoeffPeak * envelopePeak + (1.0f - attackCoeffPeak) * rectified;
    else
        envelopePeak = releaseCoeff * envelopePeak + (1.0f - releaseCoeff) * rectified;

    const float envelope = juce::jmax (envelopeFast, envelopePeak);

    // Parallel main+side law: the side path's gain collapses toward unity
    // as the envelope approaches the limiter threshold, so the boost is
    // concentrated on low-level content exactly as in the encoder-only mod.
    const float levelNorm = juce::jlimit (0.0f, 1.0f, envelope / juce::jmax (thresholdLinear, 1.0e-6f));
    const float knee = 1.0f - levelNorm;
    const float gain = 1.0f + (boostLinearMax - 1.0f) * knee * knee;

    // Return only the "air" contribution (gain above unity), softly
    // clipped so a sibilant burst that beats the detector doesn't spike.
    return softClip (band * (gain - 1.0f) * 0.5f) * 2.0f;
}

void AirBand::process (const float* input, float* airOut, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        airOut[i] = processSample (input[i]);
}

void AirBandDSP::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    midBands.resize ((size_t) numChannels);
    highBands.resize ((size_t) numChannels);

    for (auto& band : midBands)
        band.prepare (sampleRate, maxBlockSize, 3000.0f);

    for (auto& band : highBands)
        band.prepare (sampleRate, maxBlockSize, 9000.0f);

    midAir.setSize (numChannels, maxBlockSize);
    highAir.setSize (numChannels, maxBlockSize);
}

void AirBandDSP::reset()
{
    for (auto& band : midBands)
        band.reset();

    for (auto& band : highBands)
        band.reset();
}

void AirBandDSP::setParameters (float midBoostDb, float highBoostDb, float blend, float outputGainDb)
{
    // Threshold set so the boost has fully collapsed by roughly -6 dBFS in
    // the band, matching the historical unit's behaviour of leaving loud
    // high-frequency material essentially untouched.
    for (auto& band : midBands)
        band.setParameters (midBoostDb, -6.0f);

    for (auto& band : highBands)
        band.setParameters (highBoostDb, -6.0f);

    blendAmount = blend;
    outputGainLinear = juce::Decibels::decibelsToGain (outputGainDb);
}

void AirBandDSP::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (midAir.getNumSamples() < numSamples || midAir.getNumChannels() < numChannels)
    {
        midAir.setSize (numChannels, numSamples, false, false, true);
        highAir.setSize (numChannels, numSamples, false, false, true);
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        auto* midOut = midAir.getWritePointer (ch);
        auto* highOut = highAir.getWritePointer (ch);

        midBands[(size_t) ch].process (channelData, midOut, numSamples);
        highBands[(size_t) ch].process (channelData, highOut, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = channelData[i];
            const float air = (midOut[i] + highOut[i]) * blendAmount;
            channelData[i] = (dry + air) * outputGainLinear;
        }
    }
}
