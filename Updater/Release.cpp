#include "Release.h"

#include <juce_core/juce_core.h>

namespace updater
{
    const Asset* Release::find (const std::string& assetName) const
    {
        for (const auto& asset : assets)
            if (asset.name == assetName)
                return &asset;

        return nullptr;
    }

    std::optional<Release> parseRelease (const std::string& json, std::string& error)
    {
        const juce::var root = juce::JSON::parse (juce::String (json));
        if (! root.isObject())
        {
            error = "the release response is not a JSON object";
            return std::nullopt;
        }

        Release release;
        release.tag = root["tag_name"].toString().toStdString();

        const auto version = parseVersion (release.tag);
        if (! version)
        {
            error = "release tag '" + release.tag + "' is not of the form major.minor.patch";
            return std::nullopt;
        }

        release.version = *version;
        release.draft = (bool) root["draft"];
        release.prerelease = (bool) root["prerelease"];

        const auto* assets = root["assets"].getArray();
        if (assets == nullptr)
        {
            error = "the release response has no assets list";
            return std::nullopt;
        }

        for (const auto& entry : *assets)
        {
            Asset asset;
            asset.name = entry["name"].toString().toStdString();
            asset.size = (juce::int64) entry["size"];
            asset.digest = entry["digest"].toString().toStdString();
            asset.url = entry["browser_download_url"].toString().toStdString();

            if (asset.name.empty() || asset.url.empty())
            {
                error = "an asset in the release response lacks a name or a download url";
                return std::nullopt;
            }

            release.assets.push_back (std::move (asset));
        }

        return release;
    }
}
