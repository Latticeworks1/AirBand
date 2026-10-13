#pragma once

#include <string>

#include <juce_core/juce_core.h>

namespace updater
{
    // Lower-case hex SHA-256 of the file's contents.
    std::string sha256Hex (const juce::File& file);

    // True when the file hashes to the "sha256:<hex>" value the release API reports. A missing or
    // differently prefixed digest never matches, so an asset without a published digest is refused.
    bool digestMatches (const juce::File& file, const std::string& digestField);
}
