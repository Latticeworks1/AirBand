#include "Check.h"
#include "Installed.h"
#include "UpdaterSupport.h"
#include "UpdaterSuites.h"

// Reading what is installed: bundle versions, the oldest-wins rule, and which scope holds the plugin.
namespace tests
{
    namespace
    {
        using namespace updater;

        void place (const juce::File& base, Os os, Scope scope, const std::string& vst3, const std::string& au = "")
        {
            const auto bundles = bundleLocations (os, scope, base);
            writePlist (bundles[0].path, vst3);
            if (! au.empty())
                writePlist (bundles[1].path, au);
        }

        void inspection()
        {
            section ("Inspect 1: versions come from Info.plist");
            TempDir temp;
            check (! inspect (Os::macOS, Scope::user, temp.dir).anyPresent(), "an empty folder holds nothing");
            check (! inspect (Os::macOS, Scope::user, temp.dir).oldest(), "and has no version");

            place (temp.dir, Os::macOS, Scope::user, "1.3.1", "1.3.2");
            const auto found = inspect (Os::macOS, Scope::user, temp.dir);
            check (found.anyPresent() && ! found.anyUnreadable(), "both bundles are found and readable");
            check (found.bundles[0].version == Version { 1, 3, 1 } && found.bundles[1].version == Version { 1, 3, 2 }, "each bundle reports its own version");
            check (found.oldest() == Version { 1, 3, 1 }, "a partly updated install reads as the older release");

            section ("Inspect 2: a bundle whose version cannot be read is flagged, not guessed");
            TempDir broken;
            writePlist (bundleLocations (Os::macOS, Scope::user, broken.dir)[0].path, "1.3.2", "SomeOtherKey");
            const auto unreadable = inspect (Os::macOS, Scope::user, broken.dir);
            check (unreadable.anyPresent() && unreadable.anyUnreadable() && ! unreadable.oldest(), "unreadable and without a version");
            check (! unreadable.bundles[0].problem.empty(), "with a reason");
        }

        void scopes()
        {
            section ("Scope: the existing installation decides, and two copies are an error");
            TempDir user, system;
            std::string error;

            check (detectScope (Os::macOS, user.dir, system.dir, error) == Scope::user, "nothing installed -> user scope on macOS");
            check (detectScope (Os::windows, user.dir, system.dir, error) == Scope::system, "nothing installed -> system scope on Windows");

            place (system.dir, Os::macOS, Scope::system, "1.3.2");
            check (detectScope (Os::macOS, user.dir, system.dir, error) == Scope::system, "a system copy is updated in the system scope");

            place (user.dir, Os::macOS, Scope::user, "1.3.2");
            check (! detectScope (Os::macOS, user.dir, system.dir, error) && error.find ("both") != std::string::npos, "copies in both scopes are refused");

            TempDir onlyUser, emptySystem;
            place (onlyUser.dir, Os::macOS, Scope::user, "1.3.2");
            check (detectScope (Os::macOS, onlyUser.dir, emptySystem.dir, error) == Scope::user, "a user copy is updated in the user scope");
        }
    }

    void runUpdaterInstalledTests()
    {
        inspection();
        scopes();
    }
}
