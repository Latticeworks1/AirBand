#include <cmath>
#include <cstdio>
#include <string>

#include "Check.h"
#include "HostParameters.h"

// The host boundary: parameter defaults, ranges and the percent-to-fraction conversion, exercised
// through a real AudioProcessorValueTreeState so a mistyped id or a missed /100 is caught.
namespace
{
    class StubProcessor : public juce::AudioProcessor
    {
    public:
        StubProcessor() : AudioProcessor (BusesProperties()) {}
        const juce::String getName() const override { return "stub"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    struct Fixture
    {
        StubProcessor processor;
        juce::AudioProcessorValueTreeState state { processor, nullptr, "PARAMETERS", host::createLayout() };

        void setRaw (const char* id, float value) { state.getParameter (id)->setValueNotifyingHost (state.getParameter (id)->convertTo0to1 (value)); }
        void setNormalised (float value)
        {
            for (auto* id : { host::midAirId, host::highAirId, host::blendId, host::outputId, host::deEssId, host::compId,
                              host::gateId, host::gateThresholdId, host::limiterId, host::levelTrackingId })
                state.getParameter (id)->setValueNotifyingHost (value);
        }
    };

    bool near (const AirBandSettings& a, const AirBandSettings& b)
    {
        const auto close = [] (float x, float y) { return std::abs (x - y) < 1.0e-5f; };
        return close (a.midAirDb, b.midAirDb) && close (a.highAirDb, b.highAirDb) && close (a.blend, b.blend)
               && close (a.outputDb, b.outputDb) && close (a.deEssAmount, b.deEssAmount) && close (a.compAmount, b.compAmount)
               && close (a.gateAmount, b.gateAmount) && close (a.limiterCeilingDb, b.limiterCeilingDb)
               && close (a.gateThresholdDb, b.gateThresholdDb) && a.levelTracking == b.levelTracking;
    }
}

int main()
{
    Fixture fixture;

    tests::section ("Host 1: every parameter id exists and the defaults convert to AirBandSettings{}");
    for (auto* id : { host::midAirId, host::highAirId, host::blendId, host::outputId, host::deEssId, host::compId,
                      host::gateId, host::gateThresholdId, host::limiterId, host::levelTrackingId })
        tests::check (fixture.state.getParameter (id) != nullptr, std::string ("parameter exists: ") + id);
    // JUCE stores each parameter normalised to 0-1, so a default of 0 dB on a -12..12 range comes back as
    // about -2.7e-7 dB; the tolerance is that float round trip, far below anything audible.
    tests::check (near (host::readSettings (fixture.state), AirBandSettings {}), "default host values equal the DSP defaults within 1e-5");

    tests::section ("Host 2: range minimum and maximum convert to the DSP units");
    fixture.setNormalised (0.0f);
    tests::check (near (host::readSettings (fixture.state), { 0.0f, 0.0f, 0.0f, -12.0f, 0.0f, 0.0f, 0.0f, -12.0f, -70.0f, false }), "all parameters at minimum");
    fixture.setNormalised (1.0f);
    tests::check (near (host::readSettings (fixture.state), { 15.0f, 15.0f, 1.0f, 12.0f, 1.0f, 1.0f, 1.0f, 0.0f, -20.0f, true }), "all parameters at maximum");

    tests::section ("Host 3: percent parameters scale by 1/100 and dB parameters pass through");
    fixture.setRaw (host::blendId, 37.5f);
    fixture.setRaw (host::compId, 80.0f);
    fixture.setRaw (host::outputId, -3.5f);
    const auto settings = host::readSettings (fixture.state);
    tests::check (std::abs (settings.blend - 0.375f) < 1.0e-5f && std::abs (settings.compAmount - 0.8f) < 1.0e-5f
                      && std::abs (settings.outputDb + 3.5f) < 1.0e-5f,
                  "blend 37.5% is 0.375, comp 80% is 0.8, output -3.5 dB stays -3.5 dB");

    std::printf ("\n%s\n", tests::failureCount() == 0 ? "All tests passed." : "SOME TESTS FAILED.");
    return tests::failureCount() == 0 ? 0 : 1;
}
