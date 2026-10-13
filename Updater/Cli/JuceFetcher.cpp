#include "JuceFetcher.h"

std::unique_ptr<juce::InputStream> JuceFetcher::open (const std::string& url, bool wantJson, std::string& error)
{
    int status = 0;

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withExtraHeaders (wantJson ? "User-Agent: AirBand-Updater\r\nAccept: application/vnd.github+json\r\n"
                                                         : "User-Agent: AirBand-Updater\r\n")
                             .withConnectionTimeoutMs (20000)
                             .withNumRedirectsToFollow (5)
                             .withStatusCode (&status);

    auto stream = juce::URL (juce::String (url)).createInputStream (options);
    if (stream == nullptr || status != 200)
    {
        error = "GET " + url + (status == 0 ? " got no response" : " answered HTTP " + std::to_string (status));
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
