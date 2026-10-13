#pragma once

#include <optional>
#include <string>

#include <juce_core/juce_core.h>

#include "Release.h"

namespace updater
{
    // The network seam. The command-line tool supplies a JUCE URL implementation; tests supply a fake.
    class Fetcher
    {
    public:
        virtual ~Fetcher() = default;

        virtual bool getText (const std::string& url, std::string& body, std::string& error) = 0;
        virtual bool download (const std::string& url, const juce::File& destination, std::string& error) = 0;
    };

    // The latest published release of "owner/name". Drafts and prereleases are refused.
    std::optional<Release> fetchLatestRelease (Fetcher& fetcher, const std::string& repo, std::string& error);

    // Downloads the asset and keeps the file only if its size and sha256 match the release's
    // published values; on any mismatch the file is deleted and `error` says which check failed.
    bool downloadVerified (Fetcher& fetcher, const Asset& asset, const juce::File& destination, std::string& error);
}
