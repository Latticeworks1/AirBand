#include "Decision.h"

namespace updater
{
    Action decide (const std::optional<Version>& installed, const Version& latest)
    {
        if (! installed)
            return Action::install;

        if (*installed < latest)
            return Action::update;

        return *installed == latest ? Action::upToDate : Action::newerInstalled;
    }

    const char* describe (Action action)
    {
        switch (action)
        {
            case Action::install:        return "not installed";
            case Action::update:         return "update available";
            case Action::upToDate:       return "up to date";
            case Action::newerInstalled: return "installed version is newer than the latest release";
        }

        return "";
    }
}
