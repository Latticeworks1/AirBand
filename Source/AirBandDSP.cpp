#include "AirBandDSP.h"

void AirBand::prepare (double sampleRate, int maxBlockSize, float highpassHz, bool enableDeEss)
{
    sr = sampleRate;
    highpassFreq = highpassHz;
    deEssEnabled = enableDeEss;

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

    if (deEssEnabled)
    {
        // Centred in the vocal sibilance range; Q chosen wide enough to
        // catch a whole "S" rather than a single formant peak within it.
        sibilantCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 6500.0f, 1.2f);
        sibilantFilter.coefficients = sibilantCoeffs;
        sibilantFilter.reset();

        sibilantAttackCoeff = std::exp (-1.0f / (0.001f * (float) sr));
        sibilantReleaseCoeff = std::exp (-1.0f / (0.05f * (float) sr));
    }

    oversampler.initProcessing ((size_t) maxBlockSize);

    reset();
}

void AirBand::reset()
{
    highpass.reset();
    envelopeFast = 0.0f;
    envelopePeak = 0.0f;

    if (deEssEnabled)
    {
        sibilantFilter.reset();
        sibilantEnvelope = 0.0f;
    }

    oversampler.reset();
}

void AirBand::setParameters (float boostDb, float thresholdDb, float deEssAmountIn)
{
    boostLinearMax = juce::Decibels::decibelsToGain (boostDb);
    thresholdLinear = juce::Decibels::decibelsToGain (thresholdDb);
    deEssAmount = juce::jlimit (0.0f, 1.0f, deEssAmountIn);
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
    float gain = 1.0f + (boostLinearMax - 1.0f) * knee * knee;

    if (deEssEnabled && deEssAmount > 0.0f)
    {
        const float sibilantSample = sibilantFilter.processSample (x);
        const float sibilantRectified = std::abs (sibilantSample);

        if (sibilantRectified > sibilantEnvelope)
            sibilantEnvelope = sibilantAttackCoeff * sibilantEnvelope + (1.0f - sibilantAttackCoeff) * sibilantRectified;
        else
            sibilantEnvelope = sibilantReleaseCoeff * sibilantEnvelope + (1.0f - sibilantReleaseCoeff) * sibilantRectified;

        // How much of this band's own energy sits inside the narrow
        // sibilant sub-band, as opposed to spread across the whole band
        // (breath, cymbal-like air, general high-frequency detail). Near 0
        // for broadband content, approaching 1 when the band is dominated
        // by a concentrated "S".
        const float sibilance = juce::jlimit (0.0f, 1.0f, sibilantEnvelope / juce::jmax (envelope, 1.0e-6f));
        gain *= 1.0f - deEssAmount * sibilance;
    }

    // Pre-clip "air" contribution (gain above unity); the caller runs this
    // through an oversampled soft clip afterwards so a sibilant/transient
    // burst that beats the envelope follower folds back without aliasing.
    return band * (gain - 1.0f) * 0.5f;
}

void AirBand::process (const float* input, float* airOut, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        airOut[i] = processSample (input[i]);

    float* channels[] = { airOut };
    juce::dsp::AudioBlock<float> block (channels, 1, (size_t) numSamples);

    auto oversampledBlock = oversampler.processSamplesUp (block);
    auto* osData = oversampledBlock.getChannelPointer (0);

    for (size_t i = 0; i < oversampledBlock.getNumSamples(); ++i)
        osData[i] = std::tanh (osData[i]);

    oversampler.processSamplesDown (block);

    for (int i = 0; i < numSamples; ++i)
        airOut[i] *= 2.0f;
}

void AirBandDSP::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    midBands.clear();
    highBands.clear();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        midBands.push_back (std::make_unique<AirBand>());
        midBands.back()->prepare (sampleRate, maxBlockSize, 3000.0f, false);

        highBands.push_back (std::make_unique<AirBand>());
        highBands.back()->prepare (sampleRate, maxBlockSize, 9000.0f, true);
    }

    midAir.setSize (numChannels, maxBlockSize);
    highAir.setSize (numChannels, maxBlockSize);
}

void AirBandDSP::reset()
{
    for (auto& band : midBands)
        band->reset();

    for (auto& band : highBands)
        band->reset();
}

void AirBandDSP::setParameters (float midBoostDb, float highBoostDb, float blend, float outputGainDb, float deEssAmount)
{
    // Threshold set so the boost has fully collapsed by roughly -6 dBFS in
    // the band, matching the historical unit's behaviour of leaving loud
    // high-frequency material essentially untouched.
    for (auto& band : midBands)
        band->setParameters (midBoostDb, -6.0f);

    for (auto& band : highBands)
        band->setParameters (highBoostDb, -6.0f, deEssAmount);

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

        midBands[(size_t) ch]->process (channelData, midOut, numSamples);
        highBands[(size_t) ch]->process (channelData, highOut, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = channelData[i];
            const float air = (midOut[i] + highOut[i]) * blendAmount;
            channelData[i] = (dry + air) * outputGainLinear;
        }
    }
}
