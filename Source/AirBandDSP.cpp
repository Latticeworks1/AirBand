#include "AirBandDSP.h"
#include <algorithm>
#include <complex>
#include "Sanitize.h"

void AirBandDSP::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    midBands.clear();
    highBands.clear();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        midBands.push_back (std::make_unique<AirBand>());
        midBands.back()->prepare (sampleRate, maxBlockSize, 3000.0f, kMidReferenceDb);

        highBands.push_back (std::make_unique<AirBand>());
        highBands.back()->prepare (sampleRate, maxBlockSize, 9000.0f, kHighReferenceDb);
    }

    sibilanceDetectors.clear();
    sibilanceDetectors.resize ((size_t) numChannels);
    for (auto& detector : sibilanceDetectors)
        detector.prepare (sampleRate);

    gate.prepare (sampleRate, maxBlockSize, numChannels);
    compressor.prepare (sampleRate, numChannels);
    limiter.prepare (sampleRate, maxBlockSize, numChannels);

    preparedMaxBlockSize = maxBlockSize;
    preparedChannels = numChannels;

    // Both bands use the same oversampler, so one delay lines the dry signal up with either.
    airLatency = midBands.empty() ? 0 : midBands.front()->getLatencySamples();
    dryDelay.assign ((size_t) numChannels, std::vector<float> ((size_t) airLatency, 0.0f));
    dryDelayPosition.assign ((size_t) numChannels, 0);

    midAir.setSize (numChannels, maxBlockSize);
    highAir.setSize (numChannels, maxBlockSize);
    sibilance.setSize (numChannels, maxBlockSize);

    const AirBandSettings defaults;
    blend.reset (sampleRate, kGlideSeconds);
    outputGain.reset (sampleRate, kGlideSeconds);
    blend.setCurrentAndTargetValue (defaults.blend);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (defaults.outputDb));
    blendRamp.assign ((size_t) maxBlockSize, 0.0f);
    gainRamp.assign ((size_t) maxBlockSize, 0.0f);
    parametersApplied = false;
}

void AirBandDSP::reset()
{
    for (auto& band : midBands)
        band->reset();

    for (auto& band : highBands)
        band->reset();

    for (auto& detector : sibilanceDetectors)
        detector.reset();

    for (auto& line : dryDelay)
        std::fill (line.begin(), line.end(), 0.0f);

    std::fill (dryDelayPosition.begin(), dryDelayPosition.end(), 0);

    gate.reset();
    compressor.reset();
    limiter.reset();
}

void AirBandDSP::setParameters (const AirBandSettings& settings) AIRBAND_NONBLOCKING
{
    for (auto& band : midBands)
    {
        band->setParameters (settings.midAirDb, kKneeAboveReferenceDb, settings.deEssAmount);
        band->setLevelTracking (settings.levelTracking);
    }

    for (auto& band : highBands)
    {
        band->setParameters (settings.highAirDb, kKneeAboveReferenceDb, settings.deEssAmount);
        band->setLevelTracking (settings.levelTracking);
    }

    gate.setAmount (settings.gateAmount);
    gate.setThreshold (settings.gateThresholdDb);
    compressor.setAmount (settings.compAmount);
    compressor.setLevelTracking (settings.levelTracking);
    limiter.setCeilingDb (settings.limiterCeilingDb);

    // The first call after prepare() sets the gains outright so playback does not open with a glide.
    const float gain = juce::Decibels::decibelsToGain (settings.outputDb);
    if (parametersApplied)
    {
        blend.setTargetValue (settings.blend);
        outputGain.setTargetValue (gain);
    }
    else
    {
        blend.setCurrentAndTargetValue (settings.blend);
        outputGain.setCurrentAndTargetValue (gain);
        parametersApplied = true;
    }
}

void AirBandDSP::processBlock (juce::AudioBuffer<float>& buffer) AIRBAND_NONBLOCKING
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), preparedChannels);
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || preparedMaxBlockSize == 0)
        return;

    // The chunk views the host buffer; the external-data constructor and its destructor never allocate.
    AIRBAND_UNCHECKED_BEGIN
    for (int ch = 0; ch < numChannels; ++ch)
        zeroNonFinite (buffer.getWritePointer (ch), numSamples);

    for (int start = 0; start < numSamples; start += preparedMaxBlockSize)
    {
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), numChannels, start,
                                        juce::jmin (preparedMaxBlockSize, numSamples - start));
        processChunk (chunk);
    }
    AIRBAND_UNCHECKED_END
}

void AirBandDSP::processChunk (juce::AudioBuffer<float>& buffer) AIRBAND_NONBLOCKING
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Gate breath/mouth noise before the compressor's makeup gain would
    // otherwise boost it, then level the dry signal so the air/de-ess
    // stage that follows sees a more consistent input.
    gate.process (buffer);
    compressor.process (buffer);

    for (int i = 0; i < numSamples; ++i)
    {
        blendRamp[(size_t) i] = blend.getNextValue();
        gainRamp[(size_t) i] = outputGain.getNextValue();
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        auto* midOut = midAir.getWritePointer (ch);
        auto* highOut = highAir.getWritePointer (ch);
        auto* sibilanceOut = sibilance.getWritePointer (ch);

        auto& detector = sibilanceDetectors[(size_t) ch];
        for (int i = 0; i < numSamples; ++i)
            sibilanceOut[i] = detector.process (channelData[i]);

        midBands[(size_t) ch]->process (channelData, sibilanceOut, midOut, numSamples);
        highBands[(size_t) ch]->process (channelData, sibilanceOut, highOut, numSamples);

        auto& delayLine = dryDelay[(size_t) ch];
        int position = dryDelayPosition[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            // The air contribution arrives airLatency samples late, so the dry sample it is summed with is the
            // one from airLatency samples ago: read the oldest slot of the ring, then overwrite it with the newest.
            float dry = channelData[i];
            if (airLatency > 0)
            {
                std::swap (dry, delayLine[(size_t) position]);
                position = position + 1 == airLatency ? 0 : position + 1;
            }

            const float air = (midOut[i] + highOut[i]) * blendRamp[(size_t) i];
            channelData[i] = (dry + air) * gainRamp[(size_t) i];
        }

        dryDelayPosition[(size_t) ch] = position;
    }

    // Final safety net: catches whatever the stages above stack up to.
    limiter.process (buffer);
}

float AirBandDSP::lowLevelGainDb (const AirBandSettings& settings, double frequencyHz, double sampleRate)
{
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    const auto contribution = [&] (float cornerHz, float boostDb)
    {
        const auto highpass = Coefficients::makeHighPass (sampleRate, cornerHz, 0.707f);
        const auto response = std::polar (highpass->getMagnitudeForFrequency (frequencyHz, sampleRate),
                                          highpass->getPhaseForFrequency (frequencyHz, sampleRate));
        return (double) (juce::Decibels::decibelsToGain (boostDb) - 1.0f) * response;
    };

    const auto total = 1.0 + (double) settings.blend * (contribution (3000.0f, settings.midAirDb) + contribution (9000.0f, settings.highAirDb));
    return (float) (20.0 * std::log10 (std::max (std::abs (total), 1.0e-9)));
}
