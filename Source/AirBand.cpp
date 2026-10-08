#include "AirBand.h"

void AirBand::prepare (double sampleRate, int maxBlockSize, float highpassHz, float designReferenceDb)
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
    envelope.prepare (sr, 0.002f, 0.0003f, 0.15f);

    tracker.prepare (sr, designReferenceDb);

    oversampler.initProcessing ((size_t) maxBlockSize);

    reset();
}

int AirBand::getLatencySamples() const
{
    return (int) std::lround (oversampler.getLatencyInSamples());
}

void AirBand::reset()
{
    highpass.reset();
    envelope.reset();
    tracker.reset();
    oversampler.reset();
    refreshThreshold();
}

void AirBand::refreshThreshold() AIRBAND_NONBLOCKING
{
    thresholdLinear = juce::Decibels::decibelsToGain (tracker.referenceDb() + kneeAboveReferenceDb);
}

void AirBand::setParameters (float boostDb, float kneeDb, float deEssAmountIn) AIRBAND_NONBLOCKING
{
    boostLinearMax = juce::Decibels::decibelsToGain (boostDb);
    kneeAboveReferenceDb = kneeDb;
    deEssAmount = juce::jlimit (0.0f, 1.0f, deEssAmountIn);
    refreshThreshold();
}

void AirBand::setLevelTracking (bool on) AIRBAND_NONBLOCKING
{
    tracker.setTracking (on);
}

void AirBand::restartDynamics (bool restartTracking) AIRBAND_NONBLOCKING
{
    envelope.reset();
    if (restartTracking)
        tracker.relocate();

    refreshThreshold();
}

float AirBand::processSample (float x, float sibilance) AIRBAND_NONBLOCKING
{
    AIRBAND_UNCHECKED_BEGIN
    const float band = highpass.processSample (x);
    AIRBAND_UNCHECKED_END
    const float rectified = std::abs (band);
    const float env = envelope.pushSample (rectified);

    if (tracker.push (env))
        refreshThreshold();

    // Parallel main+side law: the side path's gain collapses toward unity
    // as the envelope approaches the limiter threshold, so the boost is
    // concentrated on low-level content exactly as in the encoder-only mod.
    const float levelNorm = juce::jlimit (0.0f, 1.0f, env / juce::jmax (thresholdLinear, 1.0e-6f));
    const float knee = 1.0f - levelNorm;
    const float boost = (boostLinearMax - 1.0f) * knee * knee;

    // Pre-clip "air" contribution (gain above unity, less what De-Ess withdraws on sibilants); the
    // caller runs this through an oversampled soft clip afterwards so a sibilant/transient
    // burst that beats the envelope follower folds back without aliasing.
    return band * boost * (1.0f - deEssAmount * sibilance) * 0.5f;
}

void AirBand::process (const float* input, const float* sibilance, float* airOut, int numSamples) AIRBAND_NONBLOCKING
{
    for (int i = 0; i < numSamples; ++i)
        airOut[i] = processSample (input[i], sibilance[i]);

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
