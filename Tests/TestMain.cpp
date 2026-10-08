#include <cstdio>
#include <cstring>

#include "Check.h"
#include "Suites.h"

int main (int argc, char** argv)
{
    const bool record = argc > 1 && std::strcmp (argv[1], "--record") == 0;

    if (record)
    {
        tests::runGoldenTests (true);
        return 0;
    }

    tests::runLinearTests();
    tests::runAirTests();
    tests::runDynamicsTests();
    tests::runTrackingTests();
    tests::runPartitionTests();
    tests::runSafetyTests();
    tests::runRealtimeTests();
    tests::runGoldenTests (false);

    std::printf ("\n%s\n", tests::failureCount() == 0 ? "All tests passed." : "SOME TESTS FAILED.");
    return tests::failureCount() == 0 ? 0 : 1;
}
