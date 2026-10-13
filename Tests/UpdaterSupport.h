#pragma once

#include <string>

#include <juce_core/juce_core.h>

namespace tests
{
    // A scratch directory removed when it goes out of scope.
    struct TempDir
    {
        juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("airband-updater-test", "", false);

        TempDir() { dir.createDirectory(); }
        ~TempDir() { dir.deleteRecursively(); }
    };

    // Writes <bundle>/Contents/Info.plist in the form Xcode and JUCE produce, DOCTYPE included.
    inline void writePlist (const juce::File& bundle, const std::string& version, const std::string& versionKey = "CFBundleShortVersionString")
    {
        const auto plist = bundle.getChildFile ("Contents/Info.plist");
        plist.getParentDirectory().createDirectory();
        plist.replaceWithText (
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n<dict>\n"
            "  <key>CFBundleExecutable</key>\n  <string>AirBand</string>\n"
            "  <key>CFBundleIdentifier</key>\n  <string>com.airband.test</string>\n"
            "  <key>CFBundlePackageType</key>\n  <string>BNDL</string>\n"
            "  <key>" + juce::String (versionKey) + "</key>\n  <string>" + juce::String (version) + "</string>\n"
            "</dict>\n</plist>\n");
    }
}
