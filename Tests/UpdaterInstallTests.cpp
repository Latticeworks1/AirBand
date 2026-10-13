#include "Check.h"
#include "UpdaterSuites.h"

#if defined(__APPLE__)

#include "Installed.h"
#include "MacInstall.h"
#include "UpdaterSupport.h"

// The macOS installer against real, ad-hoc-signed bundle archives in scratch directories: the
// install itself, an upgrade, and the three ways it must leave the existing install untouched.
namespace tests
{
    namespace
    {
        using namespace updater;

        bool tool (const juce::StringArray& args)
        {
            juce::ChildProcess process;
            if (! process.start (args))
                return false;
            process.readAllProcessOutput();
            return process.waitForProcessToFinish (60000) && process.getExitCode() == 0;
        }

        // An AirBand-<version>/ folder holding both bundles, sealed ad hoc, zipped the way package_macos.sh does.
        juce::File buildArchive (const juce::File& dir, const std::string& version, bool tamperAfterSigning)
        {
            const auto root = dir.getChildFile ("AirBand-" + juce::String (version));
            for (const char* name : { "AirBand.vst3", "AirBand.component" })
            {
                const auto bundle = root.getChildFile (name);
                writePlist (bundle, version);

                const auto binary = bundle.getChildFile ("Contents/MacOS/AirBand");
                binary.getParentDirectory().createDirectory();
                juce::File ("/usr/bin/true").copyFileTo (binary);
                tool ({ "/usr/bin/codesign", "--force", "--sign", "-", "--timestamp=none", bundle.getFullPathName() });

                if (tamperAfterSigning)
                    binary.appendText ("tampered");
            }

            const auto zip = dir.getChildFile ("AirBand-macOS.zip");
            tool ({ "/usr/bin/ditto", "-c", "-k", "--keepParent", root.getFullPathName(), zip.getFullPathName() });
            return zip;
        }

        std::optional<Version> installedVersion (const juce::File& base)
        {
            return inspect (Os::macOS, Scope::user, base).oldest();
        }

        void installs()
        {
            TempDir fixtures, base;
            const auto zip110 = buildArchive (fixtures.dir.getChildFile ("a"), "1.1.0", false);
            const auto zip120 = buildArchive (fixtures.dir.getChildFile ("b"), "1.2.0", false);
            const auto tampered = buildArchive (fixtures.dir.getChildFile ("c"), "1.4.0", true);
            const auto zip150 = buildArchive (fixtures.dir.getChildFile ("d"), "1.5.0", false);
            check (zip110.existsAsFile() && zip120.existsAsFile() && tampered.existsAsFile() && zip150.existsAsFile(), "fixture archives were built");

            section ("Install 1: a fresh install and an upgrade");
            std::string error;
            check (installUserBundles (zip110, base.dir, { 1, 1, 0 }, error), "a fresh install succeeds: " + error);
            check (installedVersion (base.dir) == Version { 1, 1, 0 }, "both bundles report 1.1.0");
            check (installUserBundles (zip120, base.dir, { 1, 2, 0 }, error), "an upgrade succeeds: " + error);
            check (installedVersion (base.dir) == Version { 1, 2, 0 }, "both bundles report 1.2.0");
            check (base.dir.getChildFile ("Library/Audio/Plug-Ins/VST3").getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 1,
                   "the plugin folder holds only the new bundle");
            check (base.dir.getChildFile ("Library/Caches/AirBand-Updater").getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0,
                   "the staging directory is removed");

            section ("Install 2: an archive that does not match the release leaves the install untouched");
            check (! installUserBundles (zip120, base.dir, { 1, 4, 0 }, error) && error.find ("no AirBand") != std::string::npos,
                   "an archive without the expected AirBand-<version> folder is refused");
            check (installedVersion (base.dir) == Version { 1, 2, 0 }, "still 1.2.0");

            check (! installUserBundles (tampered, base.dir, { 1, 4, 0 }, error) && error.find ("code seal") != std::string::npos,
                   "a bundle that fails code-signature verification is refused");
            check (installedVersion (base.dir) == Version { 1, 2, 0 }, "still 1.2.0");

            section ("Install 3: a failure part-way through puts the previous copies back");
            const auto components = base.dir.getChildFile ("Library/Audio/Plug-Ins/Components");
            components.setReadOnly (true);
            const bool refused = ! installUserBundles (zip150, base.dir, { 1, 5, 0 }, error);
            components.setReadOnly (false);
            check (refused && ! error.empty(), "an unwritable AU folder fails the install: " + error);
            const auto after = inspect (Os::macOS, Scope::user, base.dir);
            check (after.bundles[0].version == Version { 1, 2, 0 }, "the VST3 that had already been replaced is back at 1.2.0");
            check (after.bundles[1].version == Version { 1, 2, 0 }, "the AU is still at 1.2.0");
        }
    }

    void runUpdaterInstallTests()
    {
        installs();
    }
}

#else

namespace tests
{
    void runUpdaterInstallTests()
    {
        section ("Install: macOS installer tests are skipped on this platform");
    }
}

#endif
