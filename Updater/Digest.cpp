#include "Digest.h"

#include <juce_cryptography/juce_cryptography.h>

namespace updater
{
    std::string sha256Hex (const juce::File& file)
    {
        return juce::SHA256 (file).toHexString().toLowerCase().toStdString();
    }

    bool digestMatches (const juce::File& file, const std::string& digestField)
    {
        const std::string prefix = "sha256:";
        if (digestField.size() <= prefix.size() || digestField.compare (0, prefix.size(), prefix) != 0)
            return false;

        return sha256Hex (file) == digestField.substr (prefix.size());
    }
}
