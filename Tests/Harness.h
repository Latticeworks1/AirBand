#pragma once

#include <vector>

#include "AirBandSettings.h"
#include "TestSignals.h"

// Runs a signal through the production AirBandDSP in host-style blocks.
namespace tests
{
    // At the first block starting at or after `atSample`, the DSP receives `settings`.
    struct ParameterStep
    {
        int atSample;
        AirBandSettings settings;
    };

    struct RenderOptions
    {
        double sampleRate = kDefaultRate;
        int maxBlockSize = 512;
        int channels = 2;

        // Block lengths cycled through until the input is consumed.
        std::vector<int> partition = { 512 };
        std::vector<ParameterStep> steps;
    };

    struct Render
    {
        Signal output;        // channel 0
        int latency = 0;
        std::vector<Signal> extraChannels; // channels 1..n-1
    };

    // Every channel carries `input`; the result holds all output channels.
    Render render (const Signal& input, const AirBandSettings& settings, const RenderOptions& options = {});
}
