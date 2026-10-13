#include "MacInstall.h"

#include <cstdio>
#include <filesystem>
#include <vector>

#include "Installed.h"
#include "Platform.h"

namespace updater
{
    namespace
    {
        juce::String capture (const juce::StringArray& args, bool* succeeded = nullptr)
        {
            juce::ChildProcess process;
            if (! process.start (args))
            {
                if (succeeded != nullptr)
                    *succeeded = false;
                return {};
            }

            auto output = process.readAllProcessOutput();
            const bool finished = process.waitForProcessToFinish (120000);
            if (succeeded != nullptr)
                *succeeded = finished && process.getExitCode() == 0;
            return output;
        }

        bool move (const juce::File& from, const juce::File& to)
        {
            std::error_code ec;
            std::filesystem::create_directories (to.getParentDirectory().getFullPathName().toStdString(), ec);
            std::filesystem::rename (from.getFullPathName().toStdString(), to.getFullPathName().toStdString(), ec);
            return ! ec;
        }

        struct Swap
        {
            juce::File target, staged, previous;
            bool hadPrevious = false;
            bool placed = false;
        };

        void rollback (std::vector<Swap>& swaps, const juce::File& discard)
        {
            for (auto it = swaps.rbegin(); it != swaps.rend(); ++it)
            {
                if (it->placed)
                    move (it->target, discard.getChildFile (it->target.getFileName()));
                if (it->hadPrevious)
                    move (it->previous, it->target);
            }
        }

        bool sealVerifies (const juce::File& bundle, std::string& problem)
        {
            bool ok = false;
            const auto output = capture ({ "/usr/bin/codesign", "--verify", "--deep", "--strict", bundle.getFullPathName() }, &ok);
            if (! ok)
                problem = output.trim().toStdString();
            return ok;
        }

        struct Cleanup
        {
            juce::File directory;
            ~Cleanup() { directory.deleteRecursively(); }
        };
    }

    bool installUserBundles (const juce::File& zip, const juce::File& base, const Version& expected, std::string& error)
    {
        const auto work = base.getChildFile ("Library/Caches/AirBand-Updater").getNonexistentChildFile ("work", "", false);
        const auto extract = work.getChildFile ("extract");
        const auto discard = work.getChildFile ("discarded");
        extract.createDirectory();
        Cleanup cleanup { work };

        bool unpacked = false;
        const auto unpackOutput = capture ({ "/usr/bin/ditto", "-x", "-k", zip.getFullPathName(), extract.getFullPathName() }, &unpacked);
        if (! unpacked)
        {
            error = "could not unpack " + zip.getFileName().toStdString() + ": " + unpackOutput.trim().toStdString();
            return false;
        }

        const auto root = extract.getChildFile ("AirBand-" + juce::String (toString (expected)));
        std::vector<Swap> swaps;

        for (const auto& location : bundleLocations (Os::macOS, Scope::user, base))
        {
            const auto name = location.path.getFileName().toStdString();
            Swap swap { location.path, root.getChildFile (location.path.getFileName()), work.getChildFile ("previous").getChildFile (location.path.getFileName()) };

            if (! swap.staged.isDirectory())
            {
                error = "the archive has no " + name;
                return false;
            }

            std::string problem;
            if (! sealVerifies (swap.staged, problem))
            {
                error = "the code seal of " + name + " does not verify: " + problem;
                return false;
            }

            const auto version = readInfoPlistVersion (swap.staged);
            if (! version || *version != expected)
            {
                error = name + " carries " + (version ? toString (*version) : std::string ("no readable version")) + ", expected " + toString (expected);
                return false;
            }

            swaps.push_back (std::move (swap));
        }

        for (const auto& swap : swaps)
        {
            const auto binary = swap.target.getChildFile ("Contents/MacOS/AirBand");
            if (binary.existsAsFile() && capture ({ "/usr/sbin/lsof", "-t", binary.getFullPathName() }).trim().isNotEmpty())
                std::fprintf (stderr, "warning: a running program has %s loaded and keeps the old version until it restarts\n", swap.target.getFileName().toRawUTF8());
        }

        const auto fail = [&] (const std::string& message)
        {
            rollback (swaps, discard);
            error = message;
            return false;
        };

        for (auto& swap : swaps)
        {
            const auto name = swap.target.getFileName().toStdString();

            if (swap.target.exists())
            {
                if (! move (swap.target, swap.previous))
                    return fail ("could not move the installed " + name + " aside");
                swap.hadPrevious = true;
            }

            if (! move (swap.staged, swap.target))
                return fail ("could not place the new " + name);
            swap.placed = true;
        }

        for (const auto& swap : swaps)
        {
            std::string problem;
            const auto name = swap.target.getFileName().toStdString();

            if (! sealVerifies (swap.target, problem))
                return fail ("the installed " + name + " fails verification: " + problem);

            const auto version = readInfoPlistVersion (swap.target);
            if (! version || *version != expected)
                return fail ("the installed " + name + " does not report " + toString (expected));
        }

        return true;
    }
}
