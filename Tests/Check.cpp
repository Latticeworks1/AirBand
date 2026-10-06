#include "Check.h"

#include <cstdio>

namespace tests
{
    namespace
    {
        int failures = 0;
    }

    void check (bool condition, const std::string& description)
    {
        std::printf ("  [%s] %s\n", condition ? "PASS" : "FAIL", description.c_str());
        if (! condition)
            ++failures;
    }

    void section (const std::string& title)
    {
        std::printf ("%s\n", title.c_str());
    }

    int failureCount()
    {
        return failures;
    }

    std::string scientific (double value)
    {
        char text[32];
        std::snprintf (text, sizeof text, "%.2e", value);
        return text;
    }
}
