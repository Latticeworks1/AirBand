#pragma once

#include <functional>
#include <string>

#include <juce_core/juce_core.h>

namespace updater
{
    // Starts `exe` with `arguments`, waits for it to exit and reports its exit code. The setup program
    // writes the shared Common Files folder, so the real runner elevates; tests supply a fake.
    using SetupRunner = std::function<bool (const juce::File& exe, const std::string& arguments, int& exitCode, std::string& error)>;

    // The silent switches of the Windows workflow's installer test, plus a log the setup program writes.
    std::string setupArguments (const juce::File& log);

    // Runs the verified setup program. Success means it exited with 0; the caller then inspects the
    // installed state, because only that confirms the version that is now on disk.
    bool runSetup (const juce::File& setup, const juce::File& log, const SetupRunner& run, std::string& error);
}
