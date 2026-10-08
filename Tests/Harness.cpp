#include "Harness.h"

#include <algorithm>
#include <cstring>

#include "AirBandDSP.h"

namespace tests
{
    Render render (const Signal& input, const AirBandSettings& settings, const RenderOptions& options)
    {
        AirBandDSP dsp;
        dsp.prepare (options.sampleRate, options.maxBlockSize, options.channels);
        dsp.setParameters (settings);

        Render result;
        result.latency = dsp.getLatencySamples();
        result.output.reserve (input.size());
        result.extraChannels.assign ((size_t) std::max (0, options.channels - 1), Signal());

        const int widest = *std::max_element (options.partition.begin(), options.partition.end());
        juce::AudioBuffer<float> buffer (options.channels, widest);

        size_t nextStep = 0;
        size_t nextEvent = 0;
        size_t position = 0;
        size_t blockIndex = 0;

        while (position < input.size())
        {
            const int length = (int) std::min<size_t> ((size_t) options.partition[blockIndex++ % options.partition.size()],
                                                       input.size() - position);

            if (nextStep < options.steps.size() && (int) position >= options.steps[nextStep].atSample)
                dsp.setParameters (options.steps[nextStep++].settings);

            if (nextEvent < options.events.size() && (int) position >= options.events[nextEvent].atSample)
                dsp.noteTransportEvent (options.events[nextEvent++].event);

            buffer.setSize (options.channels, length, false, false, true);
            for (int ch = 0; ch < options.channels; ++ch)
                std::memcpy (buffer.getWritePointer (ch), input.data() + position, sizeof (float) * (size_t) length);

            dsp.processBlock (buffer);

            result.output.insert (result.output.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + length);
            for (int ch = 1; ch < options.channels; ++ch)
                result.extraChannels[(size_t) ch - 1].insert (result.extraChannels[(size_t) ch - 1].end(),
                                                              buffer.getReadPointer (ch), buffer.getReadPointer (ch) + length);
            position += (size_t) length;
        }

        return result;
    }
}
