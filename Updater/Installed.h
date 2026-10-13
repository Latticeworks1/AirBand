#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Platform.h"
#include "Version.h"

namespace updater
{
    struct InstalledBundle
    {
        Bundle bundle;
        bool present = false;
        std::optional<Version> version;   // empty when absent or unreadable
        std::string problem;              // why a present bundle's version could not be read
    };

    struct Inspection
    {
        std::vector<InstalledBundle> bundles;

        bool anyPresent() const;
        bool anyUnreadable() const;

        // The oldest version across present bundles, so a partly updated install reads as the older release.
        std::optional<Version> oldest() const;
    };

    // CFBundleShortVersionString from <bundle>/Contents/Info.plist.
    std::optional<Version> readInfoPlistVersion (const juce::File& bundle);

    Inspection inspect (Os os, Scope scope, const juce::File& base);

    // The scope that already holds an installation, or the default scope when neither does. Two
    // installations (one per scope) make hosts list the plugin twice, so that case is an error and
    // returns nullopt with the reason in `error`.
    std::optional<Scope> detectScope (Os os, const juce::File& userBase, const juce::File& systemBase, std::string& error);
}
