#pragma once

#include <string>

namespace tests
{
    // Records one assertion; prints PASS/FAIL and counts failures.
    void check (bool condition, const std::string& description);
    void section (const std::string& title);
    int failureCount();

    // Fixed two-digit scientific notation, for drifts and differences.
    std::string scientific (double value);
}
