#pragma once

#include <compare>
#include <optional>
#include <string>

namespace updater
{
    // Release tags are plain major.minor.patch ("v1.3.2"); the bundles and installers carry the same number.
    struct Version
    {
        int major = 0;
        int minor = 0;
        int patch = 0;

        auto operator<=> (const Version&) const = default;
    };

    // Accepts "1.3.2" and "v1.3.2". Any other shape (suffixes, missing parts, signs) yields nullopt.
    std::optional<Version> parseVersion (const std::string& text);

    std::string toString (const Version& version);
}
