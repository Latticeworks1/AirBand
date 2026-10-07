#pragma once

#include <memory>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "AirBand.h"
#include "Realtime.h"
#include "AirBandSettings.h"
#include "VocalCompressor.h"
#include "VocalGate.h"
#include "VocalLimiter.h"

class AirBandDSP
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // See AirBandSettings.h for the fields. blend is the dry/processed
    // summing ratio (historical mod ran roughly 0.16-0.22); compAmount runs
    // the shared-detector compressor and gateAmount the expander/gate on the
    // dry signal (gate first, then compressor); limiterCeilingDb is the final
    // lookahead limiter, applied after everything including output gain.
    void setParameters (const AirBandSettings& settings) AIRBAND_NONBLOCKING;

    // Block sizes up to the prepared maximum run in one pass; larger blocks
    // are processed in prepared-size chunks, and channels beyond the
    // prepared count are passed through. Never allocates.
    void processBlock (juce::AudioBuffer<float>& buffer) AIRBAND_NONBLOCKING;

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

    void processChunk (juce::AudioBuffer<float>& buffer) AIRBAND_NONBLOCKING;

    int preparedMaxBlockSize = 0;
    int preparedChannels = 0;

    // Host automation arrives as steps at block boundaries; the gains glide to each new value
    // over kGlideSeconds. Per-sample values are staged in the ramps so every channel sees the same gain.
    static constexpr double kGlideSeconds = 0.02;
    juce::SmoothedValue<float> blend { 0.2f };
    juce::SmoothedValue<float> outputGain { 1.0f };
    std::vector<float> blendRamp, gainRamp;
    bool parametersApplied = false;
};
