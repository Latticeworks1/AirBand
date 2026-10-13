#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Version.h"

namespace updater
{
    // One downloadable file of a GitHub release. The digest is "sha256:<hex>" as the API reports it.
    struct Asset
    {
        std::string name;
        std::int64_t size = 0;
        std::string digest;
        std::string url;
    };

    struct Release
    {
        std::string tag;
        Version version;
        bool draft = false;
        bool prerelease = false;
        std::vector<Asset> assets;

        const Asset* find (const std::string& assetName) const;
    };

    // Reads the subset of the GitHub "latest release" response the updater depends on. On failure
    // returns nullopt and says why in `error`.
    std::optional<Release> parseRelease (const std::string& json, std::string& error);
}
