#pragma once

// Every suite declared here must be called from UpdaterTestMain.cpp.
namespace tests
{
    void runUpdaterLogicTests();
    void runUpdaterFetchTests();
    void runUpdaterInstalledTests();
    void runUpdaterInstallTests();
    void runUpdaterWindowsTests();
}
