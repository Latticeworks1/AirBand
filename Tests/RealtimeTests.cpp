#include <algorithm>

#include "AirBandDSP.h"
#include "Check.h"
#include "Harness.h"
#include "Suites.h"
#include "TestSettings.h"

// processBlock and setParameters carry the nonblocking attribute (Realtime.h). In a build with
// -fsanitize=realtime (scripts/check_realtime.sh) the sanitizer aborts the process on any
// allocation, lock or other blocking call inside them, so reaching the final check means none
// occurred. Elsewhere this is a smoke test over the same paths.
namespace tests
{
    namespace
    {
        // Runs every stage in its active region: gate and compressor engaged, limiter clamping a hot
        // transient, de-ess path live, and the oversized-block chunking path.
        void drive (const AirBandSettings& settings, int channels, int blockLength)
        {
            AirBandDSP dsp;
            dsp.prepare (48000.0, 512, channels);

            auto noise = makeNoise (blockLength, 0.3f, 21u);
            for (size_t i = 100; i < 140 && i < noise.size(); ++i)
                noise[i] = 1.0f;

            juce::AudioBuffer<float> buffer (channels, blockLength);

            for (int block = 0; block < 48; ++block)
            {
                for (int ch = 0; ch < channels; ++ch)
                    std::copy (noise.begin(), noise.end(), buffer.getWritePointer (ch));

                dsp.setParameters (block % 2 == 0 ? settings : transparentSettings());
                dsp.processBlock (buffer);
            }
        }
    }

    void runRealtimeTests()
    {
        section ("Realtime: processBlock and setParameters never allocate, lock or block");
        drive (featureSettings(), 2, 512);
        drive (maximumSettings(), 2, 512);
        drive (featureSettings(), 1, 64);
        drive (featureSettings(), 2, 1300);
        check (true, "48 blocks per configuration ran: stereo feature, stereo maximum, mono short, oversized");
    }
}
