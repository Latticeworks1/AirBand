#include <string>

#include "Check.h"
#include "Installed.h"
#include "UpdaterSuites.h"
#include "UpdaterSupport.h"
#include "WindowsSetup.h"

// The Windows half of the updater, exercised without Windows: the registry and the setup program are replaced by fakes.
namespace tests
{
    namespace
    {
        using namespace updater;

        juce::File placeVst3 (const juce::File& commonFiles)
        {
            const auto bundle = bundleLocations (Os::windows, Scope::system, commonFiles)[0].path;
            bundle.getChildFile ("Contents/x86_64-win").createDirectory();
            return bundle;
        }

        RegistryReader registryHolding (const std::string& value)
        {
            return [value] (const std::string& path) { return path == kWindowsInstalledVersionPath ? value : std::string(); };
        }

        void versionDetection()
        {
            section ("Windows 1: the installed version is the setup program's DisplayVersion");
            TempDir temp;
            check (! inspect (Os::windows, Scope::system, temp.dir, registryHolding ("1.4.0")).anyPresent(), "no folder, nothing installed even if a record exists");

            placeVst3 (temp.dir);
            const auto found = inspect (Os::windows, Scope::system, temp.dir, registryHolding ("1.4.0"));
            check (found.anyPresent() && found.oldest() == Version { 1, 4, 0 }, "folder plus record gives the recorded version");

            const auto missing = inspect (Os::windows, Scope::system, temp.dir, registryHolding (""));
            check (missing.anyUnreadable() && missing.bundles[0].problem.find ("uninstall record") != std::string::npos, "a folder without a record is flagged, not guessed");

            const auto odd = inspect (Os::windows, Scope::system, temp.dir, registryHolding ("1.4"));
            check (odd.anyUnreadable() && odd.bundles[0].problem.find ("\"1.4\"") != std::string::npos, "a malformed DisplayVersion is flagged with its text");

            check (inspect (Os::windows, Scope::system, temp.dir).anyUnreadable(), "without a registry reader the version is unknown");

            std::string error;
            TempDir user;
            check (detectScope (Os::windows, user.dir, temp.dir, error, registryHolding ("1.4.0")) == Scope::system, "an existing install selects the system scope");
        }

        void recordAgreesWithInstaller()
        {
            section ("Windows 2: the registry path matches the installer script");
            const auto script = juce::File (AIRBAND_REPO_DIR).getChildFile ("installer/AirBand.iss").loadFileAsString();
            const auto marker = script.fromFirstOccurrenceOf ("AppId={{", false, false).upToFirstOccurrenceOf ("\n", false, false).trim();
            check (marker.isNotEmpty(), "the installer script declares an AppId (read " + marker.toStdString() + ")");
            check (std::string (kWindowsInstalledVersionPath).find ("\\{" + marker.toStdString() + "_is1\\DisplayVersion") != std::string::npos,
                   "the registry path is that AppId plus _is1");
        }

        void setupRun()
        {
            section ("Windows 3: running the setup program");
            const juce::File setup ("C:\\temp\\AirBand-windows-setup.exe");
            const juce::File log ("C:\\temp\\AirBand-setup.log");

            const auto arguments = setupArguments (log);
            check (arguments.find ("/VERYSILENT") != std::string::npos && arguments.find ("/SUPPRESSMSGBOXES") != std::string::npos
                       && arguments.find ("/NORESTART") != std::string::npos,
                   "it runs silently and never restarts");
            check (arguments.find ("/LOG=\"" + log.getFullPathName().toStdString() + "\"") != std::string::npos, "and logs to a quoted path");

            juce::File seenExe;
            std::string seenArguments;
            const auto runner = [&] (int code, bool starts)
            {
                return [&, code, starts] (const juce::File& exe, const std::string& args, int& exitCode, std::string& error)
                {
                    seenExe = exe;
                    seenArguments = args;
                    exitCode = code;
                    if (! starts)
                        error = "the administrator prompt was declined";
                    return starts;
                };
            };

            std::string error;
            check (runSetup (setup, log, runner (0, true), error), "exit code 0 is success");
            check (seenExe == setup && seenArguments == arguments, "the runner receives the verified file and the silent switches");

            error.clear();
            check (! runSetup (setup, log, runner (5, true), error), "a non-zero exit code is a failure");
            check (error.find ("code 5") != std::string::npos && error.find (log.getFullPathName().toStdString()) != std::string::npos, "that names the code and the log");

            error.clear();
            check (! runSetup (setup, log, runner (0, false), error) && error == "the administrator prompt was declined", "a setup program that cannot start reports why");
        }
    }

    void runUpdaterWindowsTests()
    {
        versionDetection();
        recordAgreesWithInstaller();
        setupRun();
    }
}
