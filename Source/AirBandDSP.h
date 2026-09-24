#pragma once

#include <memory>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "EnvelopeDetector.h"
#include "VocalCompressor.h"
#include "VocalGate.h"
#include "VocalLimiter.h"

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
    // enableDeEss: also run a sibilant-band sub-detector and accept a
    // de-ess amount in setParameters. Only meaningful for a band whose
    // passband overlaps typical vocal sibilance (roughly 4-9 kHz); the mid
    // band leaves this off.
    void prepare (double sampleRate, int maxBlockSize, float highpassHz, bool enableDeEss = false);
    void reset();

    // boostDb: maximum gain applied to the quietest content in this band.
    // thresholdDb: envelope level at/above which the boost has fully
    // collapsed to unity (the limiter threshold in the patent topology).
    // deEssAmount (0-1): how much the boost is pulled back further when the
    // band's energy is concentrated in the sibilant sub-band rather than
    // spread across it. Ignored unless enableDeEss was set in prepare().
    void setParameters (float boostDb, float thresholdDb, float deEssAmount = 0.0f);

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

    // ~2 ms fast tracker, ~0.3 ms peak catch, ~150 ms program-dependent
    // release.
    EnvelopeDetector envelope;

    float boostLinearMax = 1.0f;
    float thresholdLinear = 1.0f;

    // Sibilance sub-detector: a narrow bandpass centred in the vocal "S"
    // range, plus its own fast envelope (single attack time, so fast and
    // peak trackers collapse together). Comparing this envelope against
    // the band's own broadband envelope gives a measure of how spectrally
    // concentrated (sibilant) versus broadband (airy/breathy) the current
    // content is, independent of the level-based knee above.
    bool deEssEnabled = false;
    juce::dsp::IIR::Filter<float> sibilantFilter;
    juce::dsp::IIR::Coefficients<float>::Ptr sibilantCoeffs;
    EnvelopeDetector sibilantEnvelope;
    float deEssAmount = 0.0f;

    // Oversampled soft clip on the side path only, so a sibilant/transient
    // burst that beats the envelope follower folds back gently instead of
    // aliasing. 2x is enough to push the tanh's harmonics of boosted
    // high-band content safely past Nyquist without real CPU cost.
    juce::dsp::Oversampling<float> oversampler { 1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };
};

class AirBandDSP
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // midBoostDb/highBoostDb: 0-15 dB, the "Mid Air" / "High Air" controls.
    // blend: 0-1, the dry/processed summing ratio (historical mod ran
    // roughly 0.16-0.22; exposed here as the "Air" master control).
    // deEssAmount: 0-1, how strongly concentrated sibilant energy in the
    // high band pulls its own boost back (applied to the high band only).
    // compAmount: 0-1, the shared-detector vocal compressor (see
    // VocalCompressor.h), applied to the dry signal before the air bands.
    // gateAmount: 0-1, the shared-detector expander/gate (see
    // VocalGate.h), applied to the dry signal before the compressor.
    // limiterCeilingDb: the final lookahead limiter's ceiling (see
    // VocalLimiter.h), applied after everything else including output gain.
    void setParameters (float midBoostDb, float highBoostDb, float blend, float outputGainDb,
                         float deEssAmount = 0.0f, float compAmount = 0.0f,
                         float gateAmount = 0.0f, float limiterCeilingDb = 0.0f);

    void processBlock (juce::AudioBuffer<float>& buffer);

    // The lookahead limiter delays the signal; the host must be told so
    // via AudioProcessor::setLatencySamples() or AirBand will drift out of
    // sync with unprocessed tracks.
    int getLatencySamples() const { return limiter.getLatencySamples(); }

private:
    // One filter/envelope state per channel, so a stereo signal doesn't
    // bleed state between L and R. Held by pointer because AirBand owns a
    // juce::dsp::Oversampling, which is neither copyable nor movable, so
    // the vector itself can't relocate AirBand instances directly.
    std::vector<std::unique_ptr<AirBand>> midBands;   // ~3 kHz highpass, "band 3" in the Dolby A patent
    std::vector<std::unique_ptr<AirBand>> highBands;  // ~9 kHz highpass, "band 4"

    VocalGate gate;
    VocalCompressor compressor;
    VocalLimiter limiter;

    juce::AudioBuffer<float> midAir;
    juce::AudioBuffer<float> highAir;

    float blendAmount = 0.2f;
    float outputGainLinear = 1.0f;
};
