#include "Installed.h"

#include <algorithm>

namespace updater
{
    bool Inspection::anyPresent() const
    {
        return std::any_of (bundles.begin(), bundles.end(), [] (const auto& b) { return b.present; });
    }

    bool Inspection::anyUnreadable() const
    {
        return std::any_of (bundles.begin(), bundles.end(), [] (const auto& b) { return b.present && ! b.version; });
    }

    std::optional<Version> Inspection::oldest() const
    {
        std::optional<Version> result;
        for (const auto& b : bundles)
            if (b.version && (! result || *b.version < *result))
                result = b.version;

        return result;
    }

    std::optional<Version> readInfoPlistVersion (const juce::File& bundle)
    {
        const auto plist = juce::XmlDocument::parse (bundle.getChildFile ("Contents/Info.plist"));
        if (plist == nullptr)
            return std::nullopt;

        const auto* dict = plist->getChildByName ("dict");
        if (dict == nullptr)
            return std::nullopt;

        for (const auto* element = dict->getFirstChildElement(); element != nullptr; element = element->getNextElement())
        {
            if (element->hasTagName ("key") && element->getAllSubText() == "CFBundleShortVersionString")
            {
                const auto* value = element->getNextElement();
                if (value != nullptr && value->hasTagName ("string"))
                    return parseVersion (value->getAllSubText().toStdString());
            }
        }

        return std::nullopt;
    }

    Inspection inspect (Os os, Scope scope, const juce::File& base)
    {
        Inspection result;

        for (const auto& bundle : bundleLocations (os, scope, base))
        {
            InstalledBundle installed { bundle, bundle.path.exists(), std::nullopt, {} };

            if (installed.present)
            {
                if (os == Os::macOS)
                {
                    installed.version = readInfoPlistVersion (bundle.path);
                    if (! installed.version)
                        installed.problem = "Info.plist has no readable CFBundleShortVersionString";
                }
                else
                {
                    // The Windows installer records its version in the uninstall registry key; reading it is not implemented yet.
                    installed.problem = "installed-version detection on Windows is not implemented";
                }
            }

            result.bundles.push_back (std::move (installed));
        }

        return result;
    }

    std::optional<Scope> detectScope (Os os, const juce::File& userBase, const juce::File& systemBase, std::string& error)
    {
        const bool inUser = inspect (os, Scope::user, userBase).anyPresent();
        const bool inSystem = inspect (os, Scope::system, systemBase).anyPresent();

        if (inUser && inSystem)
        {
            error = "AirBand is installed in both the user and the system plugin folders; remove one copy or pass --scope";
            return std::nullopt;
        }

        if (inUser)
            return Scope::user;

        if (inSystem)
            return Scope::system;

        return defaultScope (os);
    }
}
