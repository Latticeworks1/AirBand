#pragma once

#include <optional>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>

namespace updater
{
    enum class Os { macOS, windows };

    // user: the account's own plugin folders, no administrator rights. system: the shared folders
    // every account sees, which the .pkg and the Windows installer write.
    enum class Scope { user, system };

    Os hostOs();

    // A fresh install on macOS goes to the user folders; the Windows installer only writes the shared folder.
    Scope defaultScope (Os os);

    // The release asset that installs into the given scope, per the fixed asset names of the release
    // contract. Empty when the platform offers no installer for that scope.
    std::optional<std::string> assetName (Os os, Scope scope);

    struct Bundle
    {
        std::string format;
        juce::File path;
    };

    // Where the plugin bundles live. `base` is the home directory for the macOS user scope, the
    // filesystem root for the macOS system scope, and the Common Files directory on Windows.
    std::vector<Bundle> bundleLocations (Os os, Scope scope, const juce::File& base);
}
