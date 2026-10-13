#include "Version.h"

namespace updater
{
    namespace
    {
        bool parseComponent (const std::string& text, int& value)
        {
            if (text.empty() || text.size() > 9)
                return false;

            int result = 0;
            for (const char c : text)
            {
                if (c < '0' || c > '9')
                    return false;
                result = result * 10 + (c - '0');
            }

            value = result;
            return true;
        }
    }

    std::optional<Version> parseVersion (const std::string& text)
    {
        int parts[3] = {};
        size_t start = (! text.empty() && text[0] == 'v') ? 1 : 0;

        for (int i = 0; i < 3; ++i)
        {
            const size_t end = (i < 2) ? text.find ('.', start) : text.size();
            if (end == std::string::npos || ! parseComponent (text.substr (start, end - start), parts[i]))
                return std::nullopt;
            start = end + 1;
        }

        return Version { parts[0], parts[1], parts[2] };
    }

    std::string toString (const Version& version)
    {
        return std::to_string (version.major) + "." + std::to_string (version.minor) + "." + std::to_string (version.patch);
    }
}
