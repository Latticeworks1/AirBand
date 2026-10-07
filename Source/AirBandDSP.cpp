#include "AirBandDSP.h"
#include "Sanitize.h"

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

    gate.prepare (sampleRate, numChannels);
    compressor.prepare (sampleRate, numChannels);
    limiter.prepare (sampleRate, maxBlockSize, numChannels);

    preparedMaxBlockSize = maxBlockSize;
    preparedChannels = numChannels;

    midAir.setSize (numChannels, maxBlockSize);
    highAir.setSize (numChannels, maxBlockSize);

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

    gate.reset();
    compressor.reset();
    limiter.reset();
}

void AirBandDSP::setParameters (const AirBandSettings& settings) AIRBAND_NONBLOCKING
{
    // Threshold set so the boost has fully collapsed by roughly -6 dBFS in
    // the band, matching the historical unit's behaviour of leaving loud
    // high-frequency material essentially untouched.
    for (auto& band : midBands)
        band->setParameters (settings.midAirDb, -6.0f);

    for (auto& band : highBands)
        band->setParameters (settings.highAirDb, -6.0f, settings.deEssAmount);

    gate.setAmount (settings.gateAmount);
    compressor.setAmount (settings.compAmount);
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

        midBands[(size_t) ch]->process (channelData, midOut, numSamples);
        highBands[(size_t) ch]->process (channelData, highOut, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = channelData[i];
            const float air = (midOut[i] + highOut[i]) * blendRamp[(size_t) i];
            channelData[i] = (dry + air) * gainRamp[(size_t) i];
        }
    }

    // Final safety net: catches whatever the stages above stack up to.
    limiter.process (buffer);
}
