#pragma once

// Every suite declared here must be called from TestMain.cpp.
namespace tests
{
    void runLinearTests();
    void runAirTests();
    void runDynamicsTests();
    void runTrackingTests();
    void runPartitionTests();
    void runSafetyTests();
    void runRealtimeTests();
    void runGoldenTests (bool record);
}
