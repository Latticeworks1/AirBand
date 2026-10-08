// Low-level frequency response of the air stages: output level change of a -80 dBFS sine, per frequency.
#include <cmath>
#include <cstdio>
#include <juce_dsp/juce_dsp.h>
#include "AirBand.h"
#include "Harness.h"
#include "TestMetrics.h"
#include "TestSettings.h"
#include "TestSignals.h"
using namespace tests;
int main()
{
    constexpr int len = (int) (2.0 * kDefaultRate), settle = (int) (1.0 * kDefaultRate);
    auto delta = [&] (double f, AirBandSettings on) {
        const auto x = makeSine (f, 1.0e-4f, len);
        return rmsDb (render (x, on).output, settle) - rmsDb (render (x, transparentSettings()).output, settle);
    };
    std::printf ("freq Hz | mid15 blend.2 | mid15 blend1 | high15 blend.2 | high15 blend1\n");
    for (double f : { 500.0, 1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 6500.0, 7000.0, 8000.0, 9000.0, 10000.0, 12000.0, 14000.0, 16000.0, 18000.0 })
    {
        auto s = transparentSettings();
        auto m2 = s; m2.midAirDb = 15; m2.blend = 0.2f;
        auto m1 = s; m1.midAirDb = 15; m1.blend = 1.0f;
        auto h2 = s; h2.highAirDb = 15; h2.blend = 0.2f; h2.deEssAmount = 0;
        auto h1 = s; h1.highAirDb = 15; h1.blend = 1.0f; h1.deEssAmount = 0;
        std::printf ("%7.0f | %6.2f | %6.2f | %6.2f | %6.2f\n", f, delta (f, m2), delta (f, m1), delta (f, h2), delta (f, h1));
    }
    AirBand band;
    band.prepare (kDefaultRate, 512, 3000.0f, -31.0f);
    std::printf ("air path latency: %d samples\n", band.getLatencySamples());
    return 0;
}
