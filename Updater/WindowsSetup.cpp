#include "WindowsSetup.h"

namespace updater
{
    std::string setupArguments (const juce::File& log)
    {
        return "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /LOG=\"" + log.getFullPathName().toStdString() + "\"";
    }

    bool runSetup (const juce::File& setup, const juce::File& log, const SetupRunner& run, std::string& error)
    {
        int exitCode = 0;
        if (! run (setup, setupArguments (log), exitCode, error))
            return false;

        if (exitCode != 0)
        {
            error = "the setup program exited with code " + std::to_string (exitCode) + "; its log is " + log.getFullPathName().toStdString();
            return false;
        }

        return true;
    }
}
