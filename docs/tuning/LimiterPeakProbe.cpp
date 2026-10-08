// Renders a raw float32 mono file (in.f32, 44.1 kHz) through AirBandDSP with both Air knobs at 9 dB, 40 percent blend,
// the compressor at 100 percent, the given output gain and limiter ceiling, and writes out.f32. limiter_peak.py runs it and
// measures the 4x interpolated peak of the result against the ceiling.
//
// build: compile against libairband_core.a as the other probes do.
#include <cstdio>
#include <vector>
#include "AirBandDSP.h"
int main (int argc, char** argv)
{
    FILE* f = std::fopen ("in.f32", "rb"); std::vector<float> in (8000000); in.resize (std::fread (in.data(), 4, in.size(), f)); std::fclose (f);
    const float ceiling = (float) std::atof (argv[1]), gain = (float) std::atof (argv[2]);
    AirBandDSP dsp; dsp.prepare (44100.0, 512, 1);
    AirBandSettings s; s.midAirDb = 9; s.highAirDb = 9; s.blend = 0.4f; s.compAmount = 1; s.outputDb = gain; s.limiterCeilingDb = ceiling;
    dsp.setParameters (s);
    std::vector<float> out (in.size()); juce::AudioBuffer<float> b (1, 512);
    for (size_t i = 0; i + 512 <= in.size(); i += 512) { std::copy (in.begin() + (long) i, in.begin() + (long) i + 512, b.getWritePointer (0)); dsp.processBlock (b); std::copy (b.getReadPointer (0), b.getReadPointer (0) + 512, out.begin() + (long) i); }
    f = std::fopen ("out.f32", "wb"); std::fwrite (out.data(), 4, out.size(), f); std::fclose (f);
}
