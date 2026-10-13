#include <cstdio>

#include "Check.h"
#include "UpdaterSuites.h"

int main()
{
    tests::runUpdaterLogicTests();
    tests::runUpdaterFetchTests();
    tests::runUpdaterInstalledTests();
    tests::runUpdaterInstallTests();
    tests::runUpdaterWindowsTests();

    std::printf ("\n%s\n", tests::failureCount() == 0 ? "All tests passed." : "SOME TESTS FAILED.");
    return tests::failureCount() == 0 ? 0 : 1;
}
