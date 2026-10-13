#include "JuceFetcher.h"

std::unique_ptr<juce::InputStream> JuceFetcher::open (const std::string& url, bool wantJson, std::string& error)
{
    int status = 0;

    // The unauthenticated API allows sixty requests an hour per address, which shared CI runners exhaust. A token in
    // GITHUB_TOKEN raises that limit; it goes to the API host only, never to the asset downloads that redirect elsewhere.
    juce::String headers = "User-Agent: AirBand-Updater\r\n";
    const auto token = juce::SystemStats::getEnvironmentVariable ("GITHUB_TOKEN", {});
    if (wantJson)
    {
        headers << "Accept: application/vnd.github+json\r\n";
        if (token.isNotEmpty() && url.rfind ("https://api.github.com/", 0) == 0)
            headers << "Authorization: Bearer " << token << "\r\n";
    }

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withExtraHeaders (headers)
                             .withConnectionTimeoutMs (20000)
                             .withNumRedirectsToFollow (5)
                             .withStatusCode (&status);

    auto stream = juce::URL (juce::String (url)).createInputStream (options);
    if (stream == nullptr || status != 200)
    {
        error = "GET " + url + (status == 0 ? " got no response" : " answered HTTP " + std::to_string (status));
        if (status == 403 || status == 429)
            error += "; GitHub limits unauthenticated requests per address, set GITHUB_TOKEN to raise the limit";
        return nullptr;
    }

    return stream;
}

bool JuceFetcher::getText (const std::string& url, std::string& body, std::string& error)
{
    const auto stream = open (url, true, error);
    if (stream == nullptr)
        return false;

    body = stream->readEntireStreamAsString().toStdString();
    return true;
}

bool JuceFetcher::download (const std::string& url, const juce::File& destination, std::string& error)
{
    const auto stream = open (url, false, error);
    if (stream == nullptr)
        return false;

    juce::FileOutputStream out (destination);
    if (out.failedToOpen())
    {
        error = "cannot write " + destination.getFullPathName().toStdString();
        return false;
    }

    char buffer[65536];
    for (;;)
    {
        const auto count = stream->read (buffer, (int) sizeof buffer);
        if (count < 0)
        {
            error = "the download of " + url + " was interrupted";
            return false;
        }

        if (count == 0)
            break;

        if (! out.write (buffer, (size_t) count))
        {
            error = "cannot write " + destination.getFullPathName().toStdString();
            return false;
        }
    }

    out.flush();
    return true;
}
