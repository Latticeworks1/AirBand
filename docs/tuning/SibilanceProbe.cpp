// Writes, for every 20 ms frame of a mono WAV, the mean level (linear amplitude) of several band envelopes made with
// the plugin's own filter and envelope-detector classes, so that candidate sibilance measures can be scored against
// frame labels derived from a spectrogram of the same recording.
//
// columns: frame centre in seconds, broadband, 3 kHz high-pass, 9 kHz high-pass, 6.5 kHz band-pass (Q 1.2),
// 4th order 4 to 9 kHz band-pass, 100 Hz to 1 kHz band-pass, and the output of the plugin's SibilanceDetector
//
// usage: SibilanceProbe in.wav frames.csv
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "EnvelopeDetector.h"
#include "SibilanceDetector.h"

namespace
{
    using Filter = juce::dsp::IIR::Filter<float>;

    struct Chain
    {
        Filter first, second;
        bool cascaded = false;
        EnvelopeDetector envelope;

        void prepare (double rate, juce::dsp::IIR::Coefficients<float>::Ptr a, juce::dsp::IIR::Coefficients<float>::Ptr b = nullptr)
        {
            first.coefficients = a;
            cascaded = b != nullptr;
            if (cascaded) second.coefficients = b;
            envelope.prepare (rate, 0.001f, 0.001f, 0.05f);
        }

        float push (float x)
        {
            float y = first.processSample (x);
            if (cascaded) y = second.processSample (y);
            return envelope.pushSample (std::abs (y));
        }
    };
}

int main (int argc, char** argv)
{
    if (argc < 3) return 1;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (argv[1])));
    if (reader == nullptr) return 1;
    const double rate = reader->sampleRate;
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
    const float* x = buffer.getReadPointer (0);
    const int n = buffer.getNumSamples();

    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    Chain broad, hp3, hp9, bp65, bp49, mid;
    broad.prepare (rate, Coeffs::makeAllPass (rate, 1000.0f));
    hp3.prepare (rate, Coeffs::makeHighPass (rate, 3000.0f, 0.707f));
    hp9.prepare (rate, Coeffs::makeHighPass (rate, 9000.0f, 0.707f));
    bp65.prepare (rate, Coeffs::makeBandPass (rate, 6500.0f, 1.2f));
    bp49.prepare (rate, Coeffs::makeBandPass (rate, 6000.0f, 1.5f), Coeffs::makeBandPass (rate, 6000.0f, 1.5f));
    mid.prepare (rate, Coeffs::makeBandPass (rate, 316.0f, 0.35f));

    const int hop = (int) std::lround (0.02 * rate), half = hop / 2;
    SibilanceDetector detector;
    detector.prepare (rate);
    std::vector<double> sums (7, 0.0);
    int count = 0;
    std::FILE* out = std::fopen (argv[2], "w");
    if (out == nullptr) return 1;
    std::fprintf (out, "time,broad,hp3,hp9,bp65,bp49,mid,detector\n");

    for (int i = 0; i < n; ++i)
    {
        const double values[] = { broad.push (x[i]), hp3.push (x[i]), hp9.push (x[i]), bp65.push (x[i]), bp49.push (x[i]), mid.push (x[i]), (double) detector.process (x[i]) };
        // Frame k is centred on sample k * hop, as the spectrogram frames are.
        const int centre = ((i + half) / hop) * hop;
        if (std::abs (i - centre) < half)
        {
            for (int c = 0; c < 7; ++c) sums[(size_t) c] += values[c];
            ++count;
        }
        if (i + 1 < n && ((i + 1 + half) / hop) * hop != centre)
        {
            std::fprintf (out, "%.4f", (double) centre / rate);
            for (int c = 0; c < 7; ++c) std::fprintf (out, ",%.9g", sums[(size_t) c] / std::max (1, count));
            std::fprintf (out, "\n");
            std::fill (sums.begin(), sums.end(), 0.0);
            count = 0;
        }
    }
    std::fclose (out);
    return 0;
}
