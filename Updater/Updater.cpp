#include "Updater.h"

#include "Digest.h"

namespace updater
{
    std::optional<Release> fetchLatestRelease (Fetcher& fetcher, const std::string& repo, std::string& error)
    {
        std::string body;
        if (! fetcher.getText ("https://api.github.com/repos/" + repo + "/releases/latest", body, error))
            return std::nullopt;

        auto release = parseRelease (body, error);
        if (! release)
            return std::nullopt;

        if (release->draft || release->prerelease)
        {
            error = "release " + release->tag + " is a draft or prerelease";
            return std::nullopt;
        }

        return release;
    }

    bool downloadVerified (Fetcher& fetcher, const Asset& asset, const juce::File& destination, std::string& error)
    {
        destination.getParentDirectory().createDirectory();
        destination.deleteFile();

        if (! fetcher.download (asset.url, destination, error))
        {
            destination.deleteFile();
            return false;
        }

        if (asset.size > 0 && destination.getSize() != asset.size)
        {
            error = asset.name + " is " + std::to_string (destination.getSize()) + " bytes, the release lists " + std::to_string (asset.size);
            destination.deleteFile();
            return false;
        }

        if (! digestMatches (destination, asset.digest))
        {
            error = asset.digest.empty() ? "the release publishes no sha256 digest for " + asset.name + ", so it is not installed"
                                         : asset.name + " does not match its published digest " + asset.digest;
            destination.deleteFile();
            return false;
        }

        return true;
    }
}
