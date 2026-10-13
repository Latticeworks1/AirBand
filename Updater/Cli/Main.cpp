#include <cstdio>

#include "Decision.h"
#include "Installed.h"
#include "JuceFetcher.h"
#include "Updater.h"

#include "WindowsSetup.h"

#if defined(__APPLE__)
 #include "MacInstall.h"
#endif

#if defined(_WIN32)
 #include "WindowsNative.h"
#endif

// airband-updater status  [--scope user|system] [--prefix DIR]
// airband-updater check   [--scope user|system] [--prefix DIR] [--repo owner/name]
// airband-updater install [--scope user|system] [--prefix DIR] [--repo owner/name] [--force] [--dry-run]
//
// Exit status: 0 success or nothing to do, 10 from `check` when an install or update is available, 1 on any error.
namespace
{
    using namespace updater;

    const char* const usage =
        "usage: airband-updater status|check|install [--scope user|system] [--prefix DIR] [--repo owner/name] [--force] [--dry-run]\n"
        "  status   print where AirBand is installed and which version\n"
        "  check    also compare with the latest release; exits 10 when an install or update is available\n"
        "  install  download, verify and install the latest release (--dry-run stops before the download)\n"
        "  --scope  user or system; by default the scope that already holds AirBand (macOS user, Windows system)\n"
        "  --prefix redirect every path to DIR, for tests (macOS only; Windows installs to the shared folder)\n"
        "  --force  reinstall the same version, or replace a newer development build\n";

    struct Options
    {
        std::string command;
        std::optional<Scope> scope;
        std::optional<juce::File> prefix;
        std::string repo = "Latticeworks1/AirBand";
        bool force = false;
        bool dryRun = false;
        bool help = false;
    };

    struct Context
    {
        Os os = hostOs();
        Scope scope = Scope::user;
        juce::File userBase, systemBase;
        RegistryReader registry;

        juce::File base() const { return scope == Scope::user ? userBase : systemBase; }
        Inspection inspect() const { return updater::inspect (os, scope, base(), registry); }
    };

    int fail (const std::string& message)
    {
        std::fprintf (stderr, "error: %s\n", message.c_str());
        return 1;
    }

    bool parseOptions (int argc, char** argv, Options& options, std::string& error)
    {
        if (argc < 2)
        {
            error = std::string ("a command is required\n") + usage;
            return false;
        }

        options.command = argv[1];

        if (options.command == "--help" || options.command == "-h" || options.command == "help")
        {
            options.help = true;
            return true;
        }

        if (options.command != "status" && options.command != "check" && options.command != "install")
        {
            error = "unknown command " + options.command + "\n" + usage;
            return false;
        }

        for (int i = 2; i < argc; ++i)
        {
            const std::string arg = argv[i];
            const auto value = [&] { return i + 1 < argc ? std::string (argv[++i]) : std::string(); };

            if (arg == "--scope")
            {
                const auto scope = value();
                if (scope != "user" && scope != "system")
                {
                    error = "--scope takes user or system";
                    return false;
                }
                options.scope = scope == "user" ? Scope::user : Scope::system;
            }
            else if (arg == "--prefix")      options.prefix = juce::File (juce::String (value()));
            else if (arg == "--repo")        options.repo = value();
            else if (arg == "--force")       options.force = true;
            else if (arg == "--dry-run")     options.dryRun = true;
            else
            {
                error = "unknown option " + arg;
                return false;
            }
        }

        return true;
    }

    bool resolve (const Options& options, Context& context, std::string& error)
    {
#if defined(_WIN32)
        if (options.prefix)
        {
            error = "--prefix is not supported on Windows: the setup program writes the shared VST3 folder and the registry";
            return false;
        }

        context.registry = readRegistryValue;
#endif

        context.userBase = options.prefix.value_or (juce::File::getSpecialLocation (juce::File::userHomeDirectory));
        context.systemBase = options.prefix.value_or (context.os == Os::macOS ? juce::File ("/")
                                                                              : juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory).getChildFile ("Common Files"));

        if (options.scope)
        {
            context.scope = *options.scope;
            return true;
        }

        // A prefix names the one place to look, so both scopes share it and there is nothing to detect.
        if (options.prefix)
        {
            context.scope = defaultScope (context.os);
            return true;
        }

        const auto scope = detectScope (context.os, context.userBase, context.systemBase, error, context.registry);
        if (scope)
            context.scope = *scope;
        return scope.has_value();
    }

    void printInspection (const Context& context, const Inspection& inspection)
    {
        std::printf ("scope: %s\n", context.scope == Scope::user ? "user" : "system");

        for (const auto& b : inspection.bundles)
        {
            const auto state = ! b.present ? std::string ("absent") : b.version ? toString (*b.version) : "unreadable (" + b.problem + ")";
            std::printf ("  %-5s %s  %s\n", b.bundle.format.c_str(), b.bundle.path.getFullPathName().toRawUTF8(), state.c_str());
        }
    }

    // Downloads the asset into a scratch directory, checks it against the release, and hands it to the platform's installer.
    int install (const Context& context, const Options& options, const Release& release, Action action)
    {
        if (action == Action::upToDate && ! options.force)
            return std::printf ("AirBand %s is already installed\n", toString (release.version).c_str()), 0;

        if (action == Action::newerInstalled && ! options.force)
            return fail ("the installed version is newer than the latest release; pass --force to replace it");

        const auto name = assetName (context.os, context.scope);
        const auto* asset = name ? release.find (*name) : nullptr;
        if (asset == nullptr)
            return fail ("release " + release.tag + " has no installer asset for this platform and scope");

        const bool macUser = context.os == Os::macOS && context.scope == Scope::user;
        const bool windowsSystem = context.os == Os::windows && context.scope == Scope::system;

        if (! macUser && ! windowsSystem)
            return fail ("installing " + asset->name + " is not implemented yet; download " + asset->url);

        std::printf ("%s AirBand %s from %s (%lld bytes)\n", options.dryRun ? "would install" : "installing", toString (release.version).c_str(),
                     asset->name.c_str(), (long long) asset->size);
        if (options.dryRun)
            return 0;

        JuceFetcher fetcher;
        const auto staging = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("AirBand-Updater", "", false);
        const auto file = staging.getChildFile (asset->name);
        const auto log = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("AirBand-setup.log");
        std::string error;

        bool installed = downloadVerified (fetcher, *asset, file, error);

        if (installed && macUser)
        {
#if defined(__APPLE__)
            installed = installUserBundles (file, context.base(), release.version, error);
#endif
        }
        else if (installed && windowsSystem)
        {
#if defined(_WIN32)
            installed = runSetup (file, log, runElevated, error);
#endif
        }

        staging.deleteRecursively();
        if (! installed)
            return fail (error);

        // Only the installed state confirms what is now on disk.
        const auto after = context.inspect();
        if (after.oldest() != release.version)
            return fail ("after installing, the plugin folder does not report " + toString (release.version));

        log.deleteFile();
        printInspection (context, after);
        return 0;
    }
}

int main (int argc, char** argv)
{
    JUCE_AUTORELEASEPOOL
    {
        Options options;
        Context context;
        std::string error;

        if (! parseOptions (argc, argv, options, error))
            return fail (error);

        if (options.help)
            return std::fputs (usage, stdout), 0;

        if (! resolve (options, context, error))
            return fail (error);

        const auto inspection = context.inspect();
        printInspection (context, inspection);

        if (options.command == "status")
            return 0;

        if (inspection.anyUnreadable())
            return fail ("an installed bundle's version cannot be read; remove it or reinstall by hand");

        JuceFetcher fetcher;
        const auto release = fetchLatestRelease (fetcher, options.repo, error);
        if (! release)
            return fail (error);

        const auto action = decide (inspection.oldest(), release->version);
        std::printf ("latest: %s  %s\n", toString (release->version).c_str(), describe (action));

        if (options.command == "check")
            return (action == Action::install || action == Action::update) ? 10 : 0;

        return install (context, options, *release, action);
    }
}
