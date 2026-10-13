#include "Check.h"
#include "Digest.h"
#include "UpdaterSupport.h"
#include "UpdaterSuites.h"
#include "Updater.h"

// The download path through a fake network: a file is kept only when it matches what the release publishes.
namespace tests
{
    namespace
    {
        using namespace updater;

        // SHA-256 of "abc" (FIPS 180-2 test vector).
        const char* kAbc = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

        class FakeFetcher : public Fetcher
        {
        public:
            std::string body, payload = "abc", lastUrl;
            bool reachable = true;

            bool getText (const std::string& url, std::string& out, std::string& error) override
            {
                lastUrl = url;
                if (! reachable)
                    error = "no network";
                out = body;
                return reachable;
            }

            bool download (const std::string& url, const juce::File& destination, std::string& error) override
            {
                lastUrl = url;
                if (! reachable)
                {
                    error = "no network";
                    return false;
                }

                return destination.replaceWithData (payload.data(), payload.size());
            }
        };

        Asset abcAsset (const std::string& digest, std::int64_t size = 3)
        {
            return { "AirBand-macOS.zip", size, digest, "https://example.invalid/AirBand-macOS.zip" };
        }

        void digests()
        {
            section ("Digest: sha256 and the API's digest field");
            TempDir temp;
            const auto file = temp.dir.getChildFile ("abc.bin");
            file.replaceWithText ("abc");

            check (sha256Hex (file) == kAbc, "sha256 of 'abc' matches the published test vector");
            check (digestMatches (file, std::string ("sha256:") + kAbc), "the sha256: field matches");
            check (! digestMatches (file, kAbc), "a bare hex string is refused");
            check (! digestMatches (file, std::string ("md5:") + kAbc), "another algorithm is refused");
            check (! digestMatches (file, ""), "an empty digest is refused");
            check (! digestMatches (file, "sha256:00"), "a wrong digest is refused");
        }

        void downloads()
        {
            section ("Download: kept only when size and digest match");
            TempDir temp;
            const auto target = temp.dir.getChildFile ("out/AirBand-macOS.zip");
            const std::string good = std::string ("sha256:") + kAbc;
            FakeFetcher fetcher;
            std::string error;

            check (downloadVerified (fetcher, abcAsset (good), target, error) && target.existsAsFile(), "a matching download is kept");
            check (fetcher.lastUrl == "https://example.invalid/AirBand-macOS.zip", "the asset's own url is fetched");

            fetcher.payload = "abd";
            check (! downloadVerified (fetcher, abcAsset (good), target, error) && ! target.exists(), "tampered content is deleted");
            check (error.find ("does not match") != std::string::npos, "the error names the digest mismatch");

            fetcher.payload = "abc";
            check (! downloadVerified (fetcher, abcAsset (good, 99), target, error) && ! target.exists(), "a wrong size is deleted");
            check (! downloadVerified (fetcher, abcAsset (""), target, error) && ! target.exists(), "a missing digest is refused");
            check (error.find ("no sha256 digest") != std::string::npos, "the error says the digest is missing");

            fetcher.reachable = false;
            check (! downloadVerified (fetcher, abcAsset (good), target, error) && error == "no network", "a network failure is reported as is");
        }

        void latest()
        {
            section ("Latest release: the right url, and only a published release");
            FakeFetcher fetcher;
            std::string error;

            fetcher.body = R"({"tag_name":"v1.3.2","assets":[]})";
            const auto release = fetchLatestRelease (fetcher, "owner/name", error);
            check (release && release->tag == "v1.3.2", "the response is parsed");
            check (fetcher.lastUrl == "https://api.github.com/repos/owner/name/releases/latest", "the repo's latest-release endpoint is used");

            fetcher.body = R"({"tag_name":"v1.4.0","prerelease":true,"assets":[]})";
            check (! fetchLatestRelease (fetcher, "owner/name", error), "a prerelease is refused");

            fetcher.reachable = false;
            check (! fetchLatestRelease (fetcher, "owner/name", error) && error == "no network", "a network failure is reported as is");
        }
    }

    void runUpdaterFetchTests()
    {
        digests();
        downloads();
        latest();
    }
}
