#pragma once

#include <string>

#include <juce_core/juce_core.h>

namespace updater
{
    // The string value at a registry path (64-bit view), or empty when it does not exist.
    std::string readRegistryValue (const std::string& valuePath);

    // Starts `exe` elevated ("runas"), waits for it and reports its exit code. A refused elevation prompt is an error.
    bool runElevated (const juce::File& exe, const std::string& arguments, int& exitCode, std::string& error);
}
