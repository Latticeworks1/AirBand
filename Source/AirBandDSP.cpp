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
    envelope.prepare (sr, 0.002f, 0.0003f, 0.15f);

    if (deEssEnabled)
    {
        // Centred in the vocal sibilance range; Q chosen wide enough to
        // catch a whole "S" rather than a single formant peak within it.
        sibilantCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 6500.0f, 1.2f);
        sibilantFilter.coefficients = sibilantCoeffs;
        sibilantFilter.reset();

        sibilantEnvelope.prepare (sr, 0.001f, 0.001f, 0.05f);
    }

    oversampler.initProcessing ((size_t) maxBlockSize);

    reset();
}

void AirBand::reset()
{
    highpass.reset();
    envelope.reset();

    if (deEssEnabled)
    {
        sibilantFilter.reset();
        sibilantEnvelope.reset();
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
    const float env = envelope.pushSample (rectified);

    // Parallel main+side law: the side path's gain collapses toward unity
    // as the envelope approaches the limiter threshold, so the boost is
    // concentrated on low-level content exactly as in the encoder-only mod.
    const float levelNorm = juce::jlimit (0.0f, 1.0f, env / juce::jmax (thresholdLinear, 1.0e-6f));
    const float knee = 1.0f - levelNorm;
    float gain = 1.0f + (boostLinearMax - 1.0f) * knee * knee;

    if (deEssEnabled && deEssAmount > 0.0f)
    {
        const float sibilantSample = sibilantFilter.processSample (x);
        const float sibilantLevel = sibilantEnvelope.pushSample (std::abs (sibilantSample));

        // How much of this band's own energy sits inside the narrow
        // sibilant sub-band, as opposed to spread across the whole band
        // (breath, cymbal-like air, general high-frequency detail). Near 0
        // for broadband content, approaching 1 when the band is dominated
        // by a concentrated "S".
        const float sibilance = juce::jlimit (0.0f, 1.0f, sibilantLevel / juce::jmax (env, 1.0e-6f));
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

    compressor.prepare (sampleRate, numChannels);

    midAir.setSize (numChannels, maxBlockSize);
    highAir.setSize (numChannels, maxBlockSize);
}

void AirBandDSP::reset()
{
    for (auto& band : midBands)
        band->reset();

    for (auto& band : highBands)
        band->reset();

    compressor.reset();
}

void AirBandDSP::setParameters (float midBoostDb, float highBoostDb, float blend, float outputGainDb,
                                 float deEssAmount, float compAmount)
{
    // Threshold set so the boost has fully collapsed by roughly -6 dBFS in
    // the band, matching the historical unit's behaviour of leaving loud
    // high-frequency material essentially untouched.
    for (auto& band : midBands)
        band->setParameters (midBoostDb, -6.0f);

    for (auto& band : highBands)
        band->setParameters (highBoostDb, -6.0f, deEssAmount);

    compressor.setAmount (compAmount);

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

    // Level the dry signal first so the air/de-ess stage that follows sees
    // a more consistent input, matching typical vocal chain ordering.
    compressor.process (buffer);

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
