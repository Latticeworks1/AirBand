#include "GoldenCases.h"

#include "TestSettings.h"

namespace tests
{
    namespace
    {
        constexpr double kRate = 48000.0;
        constexpr int kSecond = 48000;

        GoldenCase make (const char* name, Signal input, AirBandSettings settings, RenderOptions options = {})
        {
            options.sampleRate = kRate;
            return { name, std::move (input), settings, std::move (options) };
        }

        Signal edgeTransients()
        {
            auto signal = makeSilence (4096);
            for (int at : { 511, 512, 1023, 2047 })
                signal[(size_t) at] = 0.8f;
            return signal;
        }
    }

    std::vector<GoldenCase> goldenCases()
    {
        const auto noise = makeNoise (kSecond, 0.3f, 1u);

        std::vector<GoldenCase> cases;
        cases.push_back (make ("impulse", makeImpulse (kSecond / 4, 100, 0.5f), featureSettings()));
        cases.push_back (make ("silence_to_transient", makeBurst (kSecond / 4, 480, kSecond / 2, 1000.0, 0.8f), featureSettings()));
        cases.push_back (make ("steady_sine", makeSine (1000.0, 0.25f, kSecond, kRate), featureSettings()));
        cases.push_back (make ("two_tone", makeTwoTone (440.0, 9000.0, 0.4f, kSecond, kRate), featureSettings()));
        cases.push_back (make ("noise", noise, featureSettings()));
        cases.push_back (make ("amplitude_steps", makeAmplitudeSteps (3000.0, { { 0.01f, 24000 }, { 0.5f, 24000 }, { 0.05f, 24000 } }, kRate),
                               featureSettings()));

        // The same program with the air knees and the compressor threshold held at their design values.
        auto pinned = featureSettings();
        pinned.levelTracking = false;
        cases.push_back (make ("amplitude_steps_pinned", makeAmplitudeSteps (3000.0, { { 0.01f, 24000 }, { 0.5f, 24000 }, { 0.05f, 24000 } }, kRate), pinned));

        RenderOptions stepped;
        stepped.steps = { { 16384, maximumSettings() }, { 32768, minimumSettings() } };
        cases.push_back (make ("parameter_steps", noise, featureSettings(), stepped));

        // The transport jumps twice and loops once: the dynamics start over each time and the trackers at the jumps.
        RenderOptions transported;
        transported.events = { { 20000, TransportEvent::jump }, { 40000, TransportEvent::loopWrap }, { 60000, TransportEvent::jump } };
        cases.push_back (make ("transport_events", makeAmplitudeSteps (3000.0, { { 0.01f, 24000 }, { 0.5f, 24000 }, { 0.05f, 24000 } }, kRate),
                               featureSettings(), transported));

        cases.push_back (make ("block_edge_transients", edgeTransients(), featureSettings()));
        cases.push_back (make ("sustained_10s", makeSine (5000.0, 0.3f, 10 * kSecond, kRate), featureSettings()));
        cases.push_back (make ("settings_minimum", noise, minimumSettings()));
        cases.push_back (make ("settings_default", noise, AirBandSettings {}));
        cases.push_back (make ("settings_maximum", noise, maximumSettings()));
        return cases;
    }
}
