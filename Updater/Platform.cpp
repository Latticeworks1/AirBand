#include "Platform.h"

namespace updater
{
    Os hostOs()
    {
#if defined(_WIN32)
        return Os::windows;
#else
        return Os::macOS;
#endif
    }

    Scope defaultScope (Os os)
    {
        return os == Os::macOS ? Scope::user : Scope::system;
    }

    std::optional<std::string> assetName (Os os, Scope scope)
    {
        if (os == Os::macOS)
            return scope == Scope::user ? "AirBand-macOS.zip" : "AirBand-macOS.pkg";

        if (scope == Scope::system)
            return "AirBand-windows-setup.exe";

        return std::nullopt;
    }

    std::vector<Bundle> bundleLocations (Os os, Scope scope, const juce::File& base)
    {
        if (os == Os::macOS)
        {
            const auto plugIns = base.getChildFile ("Library/Audio/Plug-Ins");
            return { { "VST3", plugIns.getChildFile ("VST3/AirBand.vst3") },
                     { "AU", plugIns.getChildFile ("Components/AirBand.component") } };
        }

        if (scope == Scope::system)
            return { { "VST3", base.getChildFile ("VST3/AirBand.vst3") } };

        return {};
    }
}
