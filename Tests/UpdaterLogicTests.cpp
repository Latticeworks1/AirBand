#include "Check.h"
#include "Decision.h"
#include "Platform.h"
#include "Release.h"
#include "UpdaterSuites.h"

// The pure parts of the updater: version identity and order, the update decision, the release
// contract (asset names, install locations) and the release response parser.
namespace tests
{
    namespace
    {
        using namespace updater;

        const char* kRelease = R"({"tag_name":"v1.3.2","draft":false,"prerelease":false,"assets":[
            {"name":"AirBand-macOS.zip","size":6071846,"digest":"sha256:39d73fac8ccedbce4aeb9a7e764b7a8e673979d3dd59ed8bff5b696788a1359a",
             "browser_download_url":"https://github.com/Latticeworks1/AirBand/releases/download/v1.3.2/AirBand-macOS.zip"},
            {"name":"AirBand-macOS.pkg","size":6077535,"browser_download_url":"https://example.invalid/AirBand-macOS.pkg"}]})";

        void versions()
        {
            section ("Version 1: tags parse to major.minor.patch and anything else is refused");
            const auto v = parseVersion ("v1.3.2");
            check (v && *v == Version { 1, 3, 2 }, "v1.3.2 parses");
            check (parseVersion ("1.3.2") == v, "the leading v is optional");
            for (const char* bad : { "", "v", "1.3", "1.3.2.1", "1.3.x", "-1.3.2", "1.3.2-beta", "1..2", "1.3.2 ", "1234567890.0.0" })
                check (! parseVersion (bad), std::string ("refused: '") + bad + "'");

            section ("Version 2: ordering is numeric per component");
            check (*parseVersion ("1.3.2") > *parseVersion ("1.3.1"), "1.3.2 is after 1.3.1");
            check (*parseVersion ("1.10.0") > *parseVersion ("1.9.9"), "1.10.0 is after 1.9.9 (not a string comparison)");
            check (*parseVersion ("2.0.0") > *parseVersion ("1.99.99"), "2.0.0 is after 1.99.99");
            check (toString (*parseVersion ("v1.3.2")) == "1.3.2", "toString drops the v");
        }

        void decisions()
        {
            section ("Decision: the four states");
            const Version latest { 1, 3, 2 };
            check (decide (std::nullopt, latest) == Action::install, "nothing installed -> install");
            check (decide (Version { 1, 3, 1 }, latest) == Action::update, "older -> update");
            check (decide (Version { 1, 3, 2 }, latest) == Action::upToDate, "equal -> up to date");
            check (decide (Version { 1, 4, 0 }, latest) == Action::newerInstalled, "newer -> never downgraded silently");
        }

        void contract()
        {
            section ("Contract: asset names and install locations");
            check (assetName (Os::macOS, Scope::user) == "AirBand-macOS.zip", "macOS user scope installs the zip");
            check (assetName (Os::macOS, Scope::system) == "AirBand-macOS.pkg", "macOS system scope installs the pkg");
            check (assetName (Os::windows, Scope::system) == "AirBand-windows-setup.exe", "Windows installs the setup program");
            check (! assetName (Os::windows, Scope::user), "Windows has no user-scope installer");
            check (defaultScope (Os::macOS) == Scope::user && defaultScope (Os::windows) == Scope::system, "default scopes");

            const juce::File home ("/Users/someone");
            const auto mac = bundleLocations (Os::macOS, Scope::user, home);
            check (mac.size() == 2 && mac[0].path == home.getChildFile ("Library/Audio/Plug-Ins/VST3/AirBand.vst3")
                       && mac[1].path == home.getChildFile ("Library/Audio/Plug-Ins/Components/AirBand.component"),
                   "macOS bundles sit in the Plug-Ins VST3 and Components folders under the base");
            check (bundleLocations (Os::windows, Scope::system, juce::File ("/cf")).size() == 1, "Windows has the VST3 only");
            check (bundleLocations (Os::windows, Scope::user, juce::File ("/cf")).empty(), "Windows user scope has no locations");
        }

        void releases()
        {
            section ("Release 1: the API response parses");
            std::string error;
            const auto release = parseRelease (kRelease, error);
            check (release.has_value(), "a well-formed response parses");
            if (! release)
                return;

            check (release->version == Version { 1, 3, 2 } && ! release->draft && ! release->prerelease, "tag and flags");
            const auto* zip = release->find ("AirBand-macOS.zip");
            check (zip != nullptr && zip->size == 6071846 && zip->digest.rfind ("sha256:39d73fac", 0) == 0, "asset size and digest are read");
            check (release->find ("AirBand-macOS.pkg")->digest.empty(), "a missing digest reads as empty");
            check (release->find ("nothing") == nullptr, "an unknown asset is not found");

            section ("Release 2: malformed responses are refused with a reason");
            for (const char* bad : { "not json", "[]", R"({"tag_name":"latest","assets":[]})", R"({"tag_name":"v1.0.0"})",
                                     R"({"tag_name":"v1.0.0","assets":[{"name":"a"}]})" })
            {
                std::string why;
                check (! parseRelease (bad, why) && ! why.empty(), std::string ("refused: ") + bad);
            }
        }
    }

    void runUpdaterLogicTests()
    {
        versions();
        decisions();
        contract();
        releases();
    }
}
