#pragma once

#include <memory>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "AirBand.h"
#include "Realtime.h"
#include "AirBandSettings.h"
#include "TransportMonitor.h"
#include "VocalCompressor.h"
#include "SibilanceDetector.h"
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

    // The host's transport jumped between this block and the last (see TransportMonitor). The gate, the compressor's
    // reduction, the air envelopes and the sibilance levels follow audio that is no longer playing and start over.
    // The level trackers start over as well, since the material may be a different part of the song, except after a
    // loop wrap, which brings the same material round again and keeps what they have learned of it.
    // The position on the host's timeline of the first sample of the next block, if the host reports one. Called before
    // each block and before noteTransportEvent, so that a jump can take up what the trackers learned at the new position.
    void noteTimeline (std::optional<std::int64_t> blockStart) AIRBAND_NONBLOCKING;
    void noteTransportEvent (TransportEvent event) AIRBAND_NONBLOCKING;

    // Block sizes up to the prepared maximum run in one pass; larger blocks
    // are processed in prepared-size chunks, and channels beyond the
    // prepared count are passed through. Never allocates.
    void processBlock (juce::AudioBuffer<float>& buffer) AIRBAND_NONBLOCKING;

    // The lookahead limiter and the alignment delay of the dry signal (see
    // airLatency) both delay the signal; the host must be told so via
    // AudioProcessor::setLatencySamples() or AirBand will drift out of
    // sync with unprocessed tracks.
    int getLatencySamples() const { return limiter.getLatencySamples() + airLatency; }

    // The gain in dB that the settings give a steady tone of the given frequency when it is far enough under the
    // air knees for the boost to be at its maximum (De-Ess, the compressor, the gate and the limiter play no part).
    // This is the sum of the dry path and the blended Mid and High Air paths, whose high-passes lead in phase, so
    // it is not simply the Air knobs' dB values: the knobs are the boost at 100 percent blend.
    static float lowLevelGainDb (const AirBandSettings& settings, double frequencyHz, double sampleRate);

private:
    // One filter/envelope state per channel, so a stereo signal doesn't
    // bleed state between L and R. Held by pointer because AirBand owns a
    // juce::dsp::Oversampling, which is neither copyable nor movable, so
    // the vector itself can't relocate AirBand instances directly.
    std::vector<std::unique_ptr<AirBand>> midBands;   // ~3 kHz highpass, "band 3" in the Dolby A patent
    std::vector<std::unique_ptr<AirBand>> highBands;  // ~9 kHz highpass, "band 4"

    // One sibilance measure per channel, shared by both bands.
    std::vector<SibilanceDetector> sibilanceDetectors;
    juce::AudioBuffer<float> sibilance;

    VocalGate gate;
    VocalCompressor compressor;
    VocalLimiter limiter;

    juce::AudioBuffer<float> midAir;
    juce::AudioBuffer<float> highAir;

    void processChunk (juce::AudioBuffer<float>& buffer) AIRBAND_NONBLOCKING;

    int preparedMaxBlockSize = 0;
    int preparedChannels = 0;

    // The air bands delay their contribution by airLatency samples (their oversampler's filter), so
    // the dry signal is held back by the same amount per channel before the two are summed.
    int airLatency = 0;
    std::vector<std::vector<float>> dryDelay;
    std::vector<int> dryDelayPosition;

    // Each band's boost law has its knee kKneeAboveReferenceDb above a reference level: the 90th percentile of the
    // band's own envelope (AirBand's detector, dBFS), tracked while the plugin runs and held at these design values
    // when Auto Level is off. The design values were measured on a 155 s phone vocal recording tracked at a
    // broadband median of -20 dBFS: the 3 kHz band's 90th percentile was -31.1 dBFS and the 9 kHz band's -43.0 dBFS,
    // against medians of -38.6 and -50.6 and 99th percentiles of -25.2 and -33.3. A knee 5 dB above the 90th
    // percentile is 12 dB above the median, which collapses the boost on the loudest 1 percent of the material and
    // leaves about half of it at the median, so loud consonants pass near unity and quiet detail is lifted; a knee
    // above the 99th percentile would leave the boost static. One recording; not swept by ear.
    static constexpr float kMidReferenceDb = -31.0f;
    static constexpr float kHighReferenceDb = -43.0f;
    static constexpr float kKneeAboveReferenceDb = 5.0f;

    // Host automation arrives as steps at block boundaries; the gains glide to each new value
    // over kGlideSeconds. Per-sample values are staged in the ramps so every channel sees the same gain.
    static constexpr double kGlideSeconds = 0.02;
    juce::SmoothedValue<float> blend { 0.2f };
    juce::SmoothedValue<float> outputGain { 1.0f };
    std::vector<float> blendRamp, gainRamp;
    bool parametersApplied = false;
};
