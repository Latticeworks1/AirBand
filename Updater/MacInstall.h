#pragma once

#include <string>

#include <juce_core/juce_core.h>

#include "Version.h"

namespace updater
{
    // Installs the VST3 and AU bundles from a release's AirBand-macOS.zip into <base>/Library/Audio/Plug-Ins.
    // The archive is unpacked into a staging directory on the same volume and each bundle's code seal and
    // version are checked there before any installed copy is touched. Replacement is a rename, the installed
    // copy is checked again afterwards, and any failure puts the previous copies back.
    bool installUserBundles (const juce::File& zip, const juce::File& base, const Version& expected, std::string& error);
}
