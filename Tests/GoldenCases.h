#pragma once

#include <string>
#include <vector>

#include "Harness.h"

namespace tests
{
    struct GoldenCase
    {
        std::string name;
        Signal input;
        AirBandSettings settings;
        RenderOptions options;
    };

    // The behavioural contract: fixed stimuli and settings whose outputs are recorded in GoldenVectors.h.
    std::vector<GoldenCase> goldenCases();
}
