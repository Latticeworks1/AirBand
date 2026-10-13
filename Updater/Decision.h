#pragma once

#include <optional>

#include "Version.h"

namespace updater
{
    enum class Action
    {
        install,         // nothing installed
        update,          // installed version is older than the release
        upToDate,        // installed version equals the release
        newerInstalled   // installed version is ahead of the release (a development build)
    };

    Action decide (const std::optional<Version>& installed, const Version& latest);

    const char* describe (Action action);
}
