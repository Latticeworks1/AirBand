#include "AirBand.h"

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

void AirBand::setParameters (float boostDb, float thresholdDb, float deEssAmountIn) AIRBAND_NONBLOCKING
{
    boostLinearMax = juce::Decibels::decibelsToGain (boostDb);
    thresholdLinear = juce::Decibels::decibelsToGain (thresholdDb);
    deEssAmount = juce::jlimit (0.0f, 1.0f, deEssAmountIn);
}

float AirBand::processSample (float x) AIRBAND_NONBLOCKING
{
    AIRBAND_UNCHECKED_BEGIN
    const float band = highpass.processSample (x);
    AIRBAND_UNCHECKED_END
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
        AIRBAND_UNCHECKED_BEGIN
        const float sibilantSample = sibilantFilter.processSample (x);
        AIRBAND_UNCHECKED_END
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

void AirBand::process (const float* input, float* airOut, int numSamples) AIRBAND_NONBLOCKING
{
    for (int i = 0; i < numSamples; ++i)
        airOut[i] = processSample (input[i]);

    float* channels[] = { airOut };
    juce::dsp::AudioBlock<float> block (channels, 1, (size_t) numSamples);

    AIRBAND_UNCHECKED_BEGIN
    auto oversampledBlock = oversampler.processSamplesUp (block);
    AIRBAND_UNCHECKED_END
    auto* osData = oversampledBlock.getChannelPointer (0);

    for (size_t i = 0; i < oversampledBlock.getNumSamples(); ++i)
        osData[i] = std::tanh (osData[i]);

    AIRBAND_UNCHECKED_BEGIN
    oversampler.processSamplesDown (block);
    AIRBAND_UNCHECKED_END

    for (int i = 0; i < numSamples; ++i)
        airOut[i] *= 2.0f;
}
