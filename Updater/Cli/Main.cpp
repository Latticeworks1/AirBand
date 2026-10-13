#include <cstdio>

#include "Decision.h"
#include "Installed.h"
#include "JuceFetcher.h"
#include "Updater.h"

#if defined(__APPLE__)
 #include "MacInstall.h"
#endif

// airband-updater status  [--scope user|system] [--prefix DIR]
// airband-updater check   [--scope user|system] [--prefix DIR] [--repo owner/name]
// airband-updater install [--scope user|system] [--prefix DIR] [--repo owner/name] [--force] [--dry-run]
//
// Exit status: 0 success or nothing to do, 10 from `check` when an install or update is available, 1 on any error.
namespace
{
    using namespace updater;

    struct Options
    {
        std::string command;
        std::optional<Scope> scope;
        std::optional<juce::File> prefix;
        std::string repo = "Latticeworks1/AirBand";
        bool force = false;
        bool dryRun = false;
    };

    struct Context
    {
        Os os = hostOs();
        Scope scope = Scope::user;
        juce::File userBase, systemBase;

        juce::File base() const { return scope == Scope::user ? userBase : systemBase; }
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
            error = "usage: airband-updater status|check|install [--scope user|system] [--prefix DIR] [--repo owner/name] [--force] [--dry-run]";
            return false;
        }

        options.command = argv[1];

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

        const auto scope = detectScope (context.os, context.userBase, context.systemBase, error);
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

    int install (const Context& context, const Options& options, const Release& release, const Inspection& inspection, Action action)
    {
        if (action == Action::upToDate && ! options.force)
            return std::printf ("AirBand %s is already installed\n", toString (release.version).c_str()), 0;

        if (action == Action::newerInstalled && ! options.force)
            return fail ("the installed version is newer than the latest release; pass --force to replace it");

        const auto name = assetName (context.os, context.scope);
        const auto* asset = name ? release.find (*name) : nullptr;
        if (asset == nullptr)
            return fail ("release " + release.tag + " has no installer asset for this platform and scope");

        if (context.os != Os::macOS || context.scope != Scope::user)
            return fail ("installing " + asset->name + " is not implemented yet; download " + asset->url);

        std::printf ("%s AirBand %s from %s (%lld bytes)\n", options.dryRun ? "would install" : "installing", toString (release.version).c_str(),
                     asset->name.c_str(), (long long) asset->size);
        if (options.dryRun)
            return 0;

#if defined(__APPLE__)
        JuceFetcher fetcher;
        const auto staging = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("AirBand-Updater", "", false);
        const auto zip = staging.getChildFile (asset->name);
        std::string error;

        const bool installed = downloadVerified (fetcher, *asset, zip, error) && installUserBundles (zip, context.base(), release.version, error);
        staging.deleteRecursively();
        if (! installed)
            return fail (error);

        const auto after = inspect (context.os, context.scope, context.base());
        if (after.oldest() != release.version)
            return fail ("after installing, the plugin folder does not report " + toString (release.version));

        printInspection (context, after);
        (void) inspection;
        return 0;
#else
        (void) inspection;
        return fail ("this build cannot install on this platform");
#endif
    }
}

int main (int argc, char** argv)
{
    JUCE_AUTORELEASEPOOL
    {
        Options options;
        Context context;
        std::string error;

        if (! parseOptions (argc, argv, options, error) || ! resolve (options, context, error))
            return fail (error);

        const auto inspection = inspect (context.os, context.scope, context.base());
        printInspection (context, inspection);

        if (options.command == "status")
            return 0;

        if (options.command != "check" && options.command != "install")
            return fail ("unknown command " + options.command);

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

        return install (context, options, *release, inspection, action);
    }
}
