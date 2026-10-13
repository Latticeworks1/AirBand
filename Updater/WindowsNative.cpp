#include "WindowsNative.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

namespace updater
{
    std::string readRegistryValue (const std::string& valuePath)
    {
        return juce::WindowsRegistry::getValue (juce::String (valuePath), juce::String(), juce::WindowsRegistry::WoW64_64bit).toStdString();
    }

    bool runElevated (const juce::File& exe, const std::string& arguments, int& exitCode, std::string& error)
    {
        const juce::String file = exe.getFullPathName();
        const juce::String parameters (arguments);

        SHELLEXECUTEINFOW info {};
        info.cbSize = sizeof info;
        info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
        info.lpVerb = L"runas";
        info.lpFile = file.toWideCharPointer();
        info.lpParameters = parameters.toWideCharPointer();
        info.nShow = SW_HIDE;

        if (! ShellExecuteExW (&info) || info.hProcess == nullptr)
        {
            const auto code = GetLastError();
            error = code == ERROR_CANCELLED ? std::string ("the administrator prompt was declined")
                                            : "could not start " + file.toStdString() + " (Windows error " + std::to_string (code) + ")";
            return false;
        }

        WaitForSingleObject (info.hProcess, INFINITE);

        DWORD code = 1;
        const bool read = GetExitCodeProcess (info.hProcess, &code) != 0;
        CloseHandle (info.hProcess);

        if (! read)
        {
            error = "could not read the exit code of " + file.toStdString();
            return false;
        }

        exitCode = (int) code;
        return true;
    }
}
