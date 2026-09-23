#pragma once

#include <vector>
#include <juce_dsp/juce_dsp.h>

// One companded high band: highpass split, dual-time-constant envelope
// detector, and a parallel main+side gain law modelled on the Dolby A
// encoder-only "air" mod (bands 3/4 run with no decode stage).
//
// The side path is a limiter whose contribution collapses toward zero as
// level rises, so quiet high-frequency detail gets boosted while loud
// transients pass close to unity gain. This is what makes the effect read
// as clarity rather than a static treble shelf.
class AirBand
{
public:
    void prepare (double sampleRate, int maxBlockSize, float highpassHz);
    void reset();

    // boostDb: maximum gain applied to the quietest content in this band.
    // thresholdDb: envelope level at/above which the boost has fully
    // collapsed to unity (the limiter threshold in the patent topology).
    void setParameters (float boostDb, float thresholdDb);

    // Processes one block in place, writing the band's "air" contribution
    // (already scaled by the boost/threshold law) into airOut. The caller
    // sums airOut back with the dry signal at whatever blend ratio it wants.
    void process (const float* input, float* airOut, int numSamples);

private:
    float processSample (float x);

    juce::dsp::IIR::Filter<float> highpass;
    juce::dsp::IIR::Coefficients<float>::Ptr highpassCoeffs;

    double sr = 44100.0;
    float highpassFreq = 3000.0f;

    // Dual time-constant detector: a fast integrator for general level
    // tracking, and a much faster peak catch so a transient can't punch
    // through before the slower detector reacts.
    float envelopeFast = 0.0f;
    float envelopePeak = 0.0f;
    float attackCoeffFast = 0.0f;
    float attackCoeffPeak = 0.0f;
    float releaseCoeff = 0.0f;

    float boostLinearMax = 1.0f;
    float thresholdLinear = 1.0f;

    // Light band-limited saturation on the side path only, to keep
    // sibilance/cymbal transients from overshooting the envelope follower.
    static float softClip (float x)
    {
        return std::tanh (x);
    }
};

class AirBandDSP
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // midBoostDb/highBoostDb: 0-15 dB, the "Mid Air" / "High Air" controls.
    // blend: 0-1, the dry/processed summing ratio (historical mod ran
    // roughly 0.16-0.22; exposed here as the "Air" master control).
    void setParameters (float midBoostDb, float highBoostDb, float blend, float outputGainDb);

    void processBlock (juce::AudioBuffer<float>& buffer);

private:
    // One filter/envelope state per channel, so a stereo signal doesn't
    // bleed state between L and R.
    std::vector<AirBand> midBands;   // ~3 kHz highpass, "band 3" in the Dolby A patent
    std::vector<AirBand> highBands;  // ~9 kHz highpass, "band 4"

    juce::AudioBuffer<float> midAir;
    juce::AudioBuffer<float> highAir;

    float blendAmount = 0.2f;
    float outputGainLinear = 1.0f;
};
