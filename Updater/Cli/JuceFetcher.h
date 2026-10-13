#pragma once

#include <memory>

#include "Updater.h"

// HTTPS GET through juce::URL (NSURLSession on macOS, WinINet on Windows). GitHub requires a User-Agent.
class JuceFetcher : public updater::Fetcher
{
public:
    bool getText (const std::string& url, std::string& body, std::string& error) override;
    bool download (const std::string& url, const juce::File& destination, std::string& error) override;

private:
    static std::unique_ptr<juce::InputStream> open (const std::string& url, bool wantJson, std::string& error);
};
