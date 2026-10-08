// Static and dynamic measurements of the production AirBand stages, to ground constant tuning.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <juce_dsp/juce_dsp.h>

#include "EnvelopeDetector.h"
#include "Harness.h"
#include "TestMetrics.h"
#include "TestSettings.h"
#include "TestSignals.h"

using namespace tests;

namespace
{
    constexpr int kLen = (int) (3.0 * kDefaultRate);
    constexpr int kSettle = (int) (2.0 * kDefaultRate);

    double dbfs (double db) { return std::pow (10.0, db / 20.0); }

    double delta (const Signal& in, AirBandSettings on)
    {
        return rmsDb (render (in, on).output, kSettle) - rmsDb (render (in, transparentSettings()).output, kSettle);
    }

    // Band-limited noise through the same JUCE biquad family the plugin uses.
    Signal bandNoise (double lo, double hi, float amplitude, unsigned seed)
    {
        auto x = makeNoise (kLen, amplitude, seed);
        if (lo > 0.0)
        {
            juce::dsp::IIR::Filter<float> a, b;
            a.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (kDefaultRate, lo, 0.707f);
            b.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (kDefaultRate, lo, 0.707f);
            for (auto& s : x) s = b.processSample (a.processSample (s));
        }
        if (hi > 0.0)
        {
            juce::dsp::IIR::Filter<float> a, b;
            a.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (kDefaultRate, hi, 0.707f);
            b.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (kDefaultRate, hi, 0.707f);
            for (auto& s : x) s = b.processSample (a.processSample (s));
        }
        return x;
    }

    double rms (const Signal& x, int from = 0)
    {
        double sum = 0.0;
        for (size_t i = (size_t) from; i < x.size(); ++i) sum += (double) x[i] * x[i];
        return std::sqrt (sum / (double) (x.size() - (size_t) from));
    }

    // The sibilance ratio exactly as AirBand::processSample forms it (unclamped), averaged over the last second.
    double sibilanceRatio (const Signal& x)
    {
        juce::dsp::IIR::Filter<float> hp, bp;
        hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (kDefaultRate, 9000.0f, 0.707f);
        bp.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (kDefaultRate, 6500.0f, 1.2f);
        EnvelopeDetector env, sib;
        env.prepare (kDefaultRate, 0.002f, 0.0003f, 0.15f);
        sib.prepare (kDefaultRate, 0.001f, 0.001f, 0.05f);

        double acc = 0.0;
        int n = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const float e = env.pushSample (std::abs (hp.processSample (x[i])));
            const float s = sib.pushSample (std::abs (bp.processSample (x[i])));
            if ((int) i >= (int) x.size() - (int) kDefaultRate)
            {
                acc += (double) s / std::max ((double) e, 1.0e-6);
                ++n;
            }
        }
        return acc / n;
    }
}

int main()
{
    std::printf ("== A. Air band: change in output level of a tone inside the band, vs input level ==\n");
    std::printf ("(tone inside band; stage on vs air=0; deEss 0; columns: input dBFS | mid 5kHz boost15 blend.2 | blend1.0 | high 12kHz boost15 blend.2 | blend1.0)\n");
    for (int level = -90; level <= -6; level += 6)
    {
        const auto mid = makeSine (5000.0, (float) dbfs (level), kLen);
        const auto high = makeSine (12000.0, (float) dbfs (level), kLen);
        auto s = transparentSettings();
        auto set = [&] (float midDb, float highDb, float blend) { s.midAirDb = midDb; s.highAirDb = highDb; s.blend = blend; s.deEssAmount = 0.0f; return s; };
        std::printf ("%4d | %6.2f %6.2f | %6.2f %6.2f\n", level,
                     delta (mid, set (15, 0, 0.2f)), delta (mid, set (15, 0, 1.0f)),
                     delta (high, set (0, 15, 0.2f)), delta (high, set (0, 15, 1.0f)));
    }

    std::printf ("\n== A2. Effective boost at the knob values, quietest content (-90 dBFS 5 kHz tone), by blend ==\n");
    for (float blend : { 0.1f, 0.2f, 0.4f, 1.0f })
    {
        const auto mid = makeSine (5000.0, (float) dbfs (-90), kLen);
        auto s = transparentSettings();
        s.blend = blend;
        std::printf ("blend %.1f:", blend);
        for (float knob : { 3.0f, 6.0f, 10.0f, 15.0f })
        {
            s.midAirDb = knob;
            std::printf ("  knob %4.1f dB -> %5.2f dB", knob, delta (mid, s));
        }
        std::printf ("\n");
    }

    std::printf ("\n== B. Gate: 1 kHz tone level change vs input level (amount .25 .5 1.0) ==\n");
    for (int level = -90; level <= -20; level += 5)
    {
        const auto x = makeSine (1000.0, (float) dbfs (level), kLen);
        auto s = transparentSettings();
        std::printf ("%4d |", level);
        for (float a : { 0.25f, 0.5f, 1.0f })
        {
            s.gateAmount = a;
            std::printf (" %7.2f", delta (x, s));
        }
        std::printf ("\n");
    }

    std::printf ("\n== B2. Gate decay: -20 dBFS phrase for 1 s, then a -50 dBFS tail (breath level); output-minus-input dB per 100 ms of the tail, amount 1.0 ==\n");
    {
        const int rate = (int) kDefaultRate;
        auto x = makeAmplitudeSteps (1000.0, { { (float) dbfs (-20), rate }, { (float) dbfs (-50), 2 * rate } });
        auto s = transparentSettings();
        s.gateAmount = 1.0f;
        const auto y = render (x, s).output;
        const auto ref = render (x, transparentSettings()).output;
        for (int t = 0; t < 20; ++t)
        {
            const size_t from = (size_t) rate + (size_t) t * (size_t) (rate / 10);
            const size_t to = from + (size_t) (rate / 10);
            const Signal ys (y.begin() + (long) from, y.begin() + (long) to), rs (ref.begin() + (long) from, ref.begin() + (long) to);
            std::printf ("%4d ms: %6.2f dB\n", t * 100, rmsDb (ys) - rmsDb (rs));
        }
    }

    std::printf ("\n== B3. Gate with a 60 Hz rumble at -45 dBFS under speech-level 1 kHz at -50 dBFS vs rumble alone ==\n");
    {
        auto s = transparentSettings();
        s.gateAmount = 1.0f;
        const auto rumble = makeSine (60.0, (float) dbfs (-45), kLen);
        const auto voiceOnly = makeSine (1000.0, (float) dbfs (-50), kLen);
        auto both = rumble;
        for (size_t i = 0; i < both.size(); ++i) both[i] += voiceOnly[i];
        std::printf ("1 kHz -50 dBFS alone: %.2f dB | with 60 Hz -45 dBFS: output-vs-input of whole mix %.2f dB\n", delta (voiceOnly, s), delta (both, s));
        const auto out = render (both, s).output;
        const auto ref = render (both, transparentSettings()).output;
        std::printf ("(rumble alone at -45: %.2f dB)\n", delta (rumble, s));
        (void) out; (void) ref;
    }

    std::printf ("\n== C. Compressor: 1 kHz tone level change incl. makeup vs input level (amount .25 .5 1.0) ==\n");
    for (int level = -60; level <= 0; level += 6)
    {
        const auto x = makeSine (1000.0, (float) dbfs (level), kLen);
        auto s = transparentSettings();
        std::printf ("%4d |", level);
        for (float a : { 0.25f, 0.5f, 1.0f })
        {
            s.compAmount = a;
            std::printf (" %7.2f", delta (x, s));
        }
        std::printf ("\n");
    }

    std::printf ("\n== D. Sibilance ratio as AirBand forms it (6.5 kHz bandpass level / 9 kHz highpass band level, unclamped) ==\n");
    {
        struct Case { const char* name; Signal x; };
        std::vector<Case> cases;
        cases.push_back ({ "white noise (broadband)", makeNoise (kLen, 0.1f, 11) });
        cases.push_back ({ "noise 3-6 kHz (male sibilance)", bandNoise (3000, 6000, 0.2f, 12) });
        cases.push_back ({ "noise 5-8 kHz (female sibilance)", bandNoise (5000, 8000, 0.2f, 13) });
        cases.push_back ({ "noise 4-9 kHz", bandNoise (4000, 9000, 0.2f, 14) });
        cases.push_back ({ "noise 10-16 kHz (air)", bandNoise (10000, 16000, 0.2f, 15) });
        cases.push_back ({ "sine 5 kHz", makeSine (5000.0, 0.1f, kLen) });
        cases.push_back ({ "sine 6.5 kHz", makeSine (6500.0, 0.1f, kLen) });
        cases.push_back ({ "sine 12 kHz", makeSine (12000.0, 0.1f, kLen) });
        for (const auto& c : cases)
            std::printf ("%-34s ratio %.3f\n", c.name, sibilanceRatio (c.x));
    }

    std::printf ("\n== E. What the de-ess knob does to the 9 kHz+ band: output change of band noise, highAir 15, blend .2 ==\n");
    {
        struct Case { const char* name; Signal x; };
        std::vector<Case> cases;
        cases.push_back ({ "noise 5-8 kHz", bandNoise (5000, 8000, 0.2f, 13) });
        cases.push_back ({ "noise 10-16 kHz", bandNoise (10000, 16000, 0.2f, 15) });
        for (const auto& c : cases)
        {
            std::printf ("%-18s", c.name);
            for (float d : { 0.0f, 0.5f, 1.0f })
            {
                auto s = transparentSettings();
                s.highAirDb = 15.0f;
                s.deEssAmount = d;
                std::printf ("  deEss %.1f: %6.2f dB", d, delta (c.x, s));
            }
            std::printf ("\n");
        }
    }

    std::printf ("\n== F. Mid band touches sibilance: 5-8 kHz noise through the mid band (midAir 15, blend .2), vs level ==\n");
    for (int level = -60; level <= -12; level += 12)
    {
        const auto x = bandNoise (5000, 8000, (float) dbfs (level) * 1.7f, 21);
        auto s = transparentSettings();
        s.midAirDb = 15.0f;
        std::printf ("noise rms %6.1f dBFS: %6.2f dB\n", 20.0 * std::log10 (rms (x, kSettle)), delta (x, s));
    }
    return 0;
}
