#pragma once

#include <functional>
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

    // The value the Windows setup program records for the installed version: DisplayVersion under the
    // uninstall key of its AppId (installer/AirBand.iss) plus "_is1", in the 64-bit view of HKLM.
    inline constexpr const char* kWindowsInstalledVersionPath =
        "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{28B284A1-659F-4A78-8BD0-ABD96BAEABC0}_is1\\DisplayVersion";

    // Reads the string value at a registry path and returns it empty when the value does not exist.
    // The command line supplies the real registry on Windows; tests supply a table.
    using RegistryReader = std::function<std::string (const std::string& valuePath)>;

    // CFBundleShortVersionString from <bundle>/Contents/Info.plist.
    std::optional<Version> readInfoPlistVersion (const juce::File& bundle);

    // On Windows the version comes from the registry, so `registry` is required there.
    Inspection inspect (Os os, Scope scope, const juce::File& base, const RegistryReader& registry = {});

    // The scope that already holds an installation, or the default scope when neither does. Two
    // installations (one per scope) make hosts list the plugin twice, so that case is an error and
    // returns nullopt with the reason in `error`.
    std::optional<Scope> detectScope (Os os, const juce::File& userBase, const juce::File& systemBase, std::string& error,
                                      const RegistryReader& registry = {});
}
