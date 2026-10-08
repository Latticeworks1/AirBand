#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "AirBandDSP.h"
#include "Check.h"
#include "Harness.h"
#include "Suites.h"
#include "TestMetrics.h"
#include "TestSettings.h"
#include "TransportMonitor.h"

namespace tests
{
    namespace
    {
        constexpr double kRate = kDefaultRate;
        constexpr int kBlock = 512;

        // Feeds the monitor consecutive blocks from `from`, as many as `count`, and returns the events raised.
        struct Run
        {
            TransportMonitor monitor;
            std::int64_t position = 0;

            Run() { monitor.prepare (kRate, kBlock); }

            std::optional<TransportEvent> block (bool playing = true, bool looping = false)
            {
                const auto event = monitor.observe (playing, position, looping, kBlock);
                position += kBlock;
                return event;
            }

            std::optional<TransportEvent> blockAt (std::int64_t at, bool playing = true, bool looping = false)
            {
                position = at;
                return block (playing, looping);
            }
        };

        std::string name (std::optional<TransportEvent> event)
        {
            return ! event ? "none" : *event == TransportEvent::jump ? "jump" : "loop wrap";
        }

        float gainOf (double db) { return (float) std::pow (10.0, db / 20.0); }

        Signal scaled (Signal signal, double db)
        {
            const float g = gainOf (db);
            for (auto& v : signal)
                v *= g;
            return signal;
        }

        // Mean of (output - input) frame levels over the loudest `fraction` of the 100 ms frames between two times,
        // ranked by the input's own frame level. The output is read at the reported latency.
        double loudestChangeBetween (const Signal& input, const Render& result, double fromSeconds, double toSeconds, double fraction)
        {
            const int frame = (int) (0.1 * kRate);
            std::vector<std::pair<double, double>> frames;
            for (int start = (int) (fromSeconds * kRate); start + frame <= (int) (toSeconds * kRate); start += frame)
            {
                const int skip = (int) (0.03 * kRate);
                const Samples in (input.begin() + start + skip, input.begin() + start + frame);
                const Samples out (result.output.begin() + start + skip + result.latency, result.output.begin() + start + frame + result.latency);
                frames.emplace_back (rmsDb (in), rmsDb (out) - rmsDb (in));
            }
            std::sort (frames.begin(), frames.end());
            const size_t from = (size_t) ((double) frames.size() * (1.0 - fraction));
            double total = 0.0;
            for (size_t i = from; i < frames.size(); ++i)
                total += frames[i].second;
            return total / (double) (frames.size() - from);
        }
    }

    void runTransportTests()
    {
        section ("Transport 1: consecutive blocks raise no event, and any other start is classified as a jump or a loop wrap");
        {
            Run run;
            bool quiet = true;
            for (int i = 0; i < 200; ++i)
                quiet = quiet && ! run.block();
            check (quiet, "200 consecutive blocks raise no event, the first included");

            const auto jump = run.blockAt(run.position + 10 * 44100);
            check (jump == TransportEvent::jump, "a forward jump of 10 s is a jump (" + name (jump) + ")");
            check (! run.block(), "the block after a jump continues it");

            check (run.blockAt (1000) == TransportEvent::jump, "a backward jump without looping is a jump");

            run.blockAt (200000, true, true);
            const auto wrap = run.blockAt (50000, true, true);
            check (wrap == TransportEvent::loopWrap, "a backward jump while the host loops is a loop wrap (" + name (wrap) + ")");
            check (run.blockAt (900000, true, true) == TransportEvent::jump, "a forward jump while the host loops is a jump");
        }

        section ("Transport 1b: stopped blocks and missing positions carry no information, and resuming in place is continuous");
        {
            Run run;
            for (int i = 0; i < 10; ++i)
                run.block();

            // The host stops: its position stays where it is while blocks still arrive, then it resumes at the same place.
            const auto resumeAt = run.position;
            bool silent = true;
            for (int i = 0; i < 50; ++i)
                silent = silent && ! run.blockAt (resumeAt - 4096, false);
            check (silent, "blocks reported as stopped raise no event whatever their position");
            check (! run.blockAt (resumeAt), "resuming where playback stopped is continuous");

            for (int i = 0; i < 3; ++i)
                run.block();
            run.blockAt (123456, false);
            check (run.blockAt (777777) == TransportEvent::jump, "resuming somewhere else is a jump");

            check (! run.monitor.observe (true, std::nullopt, false, kBlock), "a host that gives no position raises no event");
            check (! run.block(), "and the expected position survives a block without one");
        }

        section ("Transport 1c: positions within a block length or 20 ms of the expected one are continuous");
        {
            Run run;
            run.block();
            const auto expected = run.position;
            check (! run.blockAt (expected + 800), "800 samples late is within the tolerance");
            check (! run.blockAt (run.position - 800 + 800 - 700), "700 samples early is within the tolerance");
            check (run.blockAt (run.position + 1000) == TransportEvent::jump, "1000 samples late is a jump");

            TransportMonitor large;
            large.prepare (kRate, 4096);
            large.observe (true, 0, false, 4096);
            check (! large.observe (true, 4096 + 4000, false, 4096), "with 4096-sample blocks a position off by 4000 samples is within the tolerance");
        }

        section ("Transport 1d: the host's PositionInfo is read for samples, then seconds, and recording counts as playing");
        {
            const auto info = [] (bool playing, bool recording, bool looping, std::optional<std::int64_t> samples, std::optional<double> seconds)
            {
                juce::AudioPlayHead::PositionInfo position;
                position.setIsPlaying (playing);
                position.setIsRecording (recording);
                position.setIsLooping (looping);
                if (samples) position.setTimeInSamples (*samples);
                if (seconds) position.setTimeInSeconds (*seconds);
                return position;
            };

            TransportMonitor monitor;
            monitor.prepare (kRate, kBlock);
            check (! monitor.observe (info (true, false, false, 0, std::nullopt), kBlock), "the first block raises no event");
            check (! monitor.observe (info (true, false, false, kBlock, std::nullopt), kBlock), "samples in sequence are continuous");
            check (monitor.observe (info (true, false, false, 100000, std::nullopt), kBlock) == TransportEvent::jump, "samples out of sequence are a jump");
            check (! monitor.observe (info (false, true, false, 100000 + kBlock, std::nullopt), kBlock), "a recording host counts as playing");
            check (monitor.observe (info (true, false, true, 10, std::nullopt), kBlock) == TransportEvent::loopWrap, "looping and going back is a loop wrap");

            TransportMonitor seconds;
            seconds.prepare (kRate, kBlock);
            seconds.observe (info (true, false, false, std::nullopt, 1.0), kBlock);
            check (! seconds.observe (info (true, false, false, std::nullopt, 1.0 + (double) kBlock / kRate), kBlock), "seconds in sequence are continuous when the host gives no samples");
            check (seconds.observe (info (true, false, false, std::nullopt, 30.0), kBlock) == TransportEvent::jump, "seconds out of sequence are a jump");
            check (! seconds.observe (info (true, false, false, std::nullopt, std::nullopt), kBlock), "a host with no time at all raises no event");
        }

        section ("Transport 2: after a jump to material 12 dB quieter the compressor follows the new level within seconds, and stale references do not");
        {
            // 40 s of program-like bursts, then the same program 12 dB quieter, at full compression with level tracking on.
            const auto program = makeLevelBursts ((int) (40.0 * kRate), (int) (0.1 * kRate), -30.0, -5.0, 5u);
            auto input = program;
            const auto quiet = scaled (program, -12.0);
            input.insert (input.end(), quiet.begin(), quiet.end());

            auto settings = transparentSettings();
            settings.compAmount = 1.0f;
            settings.levelTracking = true;

            RenderOptions withJump;
            withJump.events = { { (int) (40.0 * kRate), TransportEvent::jump } };

            const auto stale = render (input, settings);
            const auto restarted = render (input, settings, withJump);

            const auto change = [&] (const Render& r, double from, double to) { return loudestChangeBetween (input, r, from, to, 0.1); };
            const double target = change (restarted, 70.0, 80.0);
            const double staleEarly = change (stale, 41.0, 44.0), staleLate = change (stale, 50.0, 60.0);
            const double restartedEarly = change (restarted, 41.0, 44.0), restartedLater = change (restarted, 45.0, 50.0);
            check (target <= -0.5, "the loudest tenth of the settled quiet half is compressed (" + std::to_string (target) + " dB)");
            check (staleEarly - target >= 2.5 && staleLate - target >= 2.5, "without the event the references still belong to the louder half, so the loudest tenth is not compressed at all 1 to 4 s ("
                                                                                + std::to_string (staleEarly) + " dB) or 10 to 20 s (" + std::to_string (staleLate) + " dB) after the splice, against "
                                                                                + std::to_string (target) + " dB once settled");
            check (std::abs (restartedEarly - target) <= 1.5, "with the event it is within 1.5 dB of the settled value 1 to 4 s after the splice (" + std::to_string (restartedEarly) + " dB)");
            check (std::abs (restartedLater - target) <= 0.5, "and within 0.5 dB of it 5 to 10 s after (" + std::to_string (restartedLater) + " dB)");
        }

        section ("Transport 3: a loop wrap keeps the references that the looped material has taught, and a jump does not");
        {
            // A 16 s loop whose first 4 s are 15 dB quieter than the rest, five passes, full compression with level tracking on.
            // A tracker restarted at each wrap learns the quiet start first and compresses it as if it were the loudest part.
            const int loop = (int) (16.0 * kRate), intro = (int) (4.0 * kRate);
            auto body = makeLevelBursts (loop, (int) (0.1 * kRate), -30.0, -8.0, 9u);
            for (int i = 0; i < intro; ++i)
                body[(size_t) i] *= gainOf (-15.0);

            Signal input;
            RenderOptions wraps, jumps;
            for (int pass = 0; pass < 5; ++pass)
            {
                if (pass > 0)
                {
                    wraps.events.push_back ({ pass * loop, TransportEvent::loopWrap });
                    jumps.events.push_back ({ pass * loop, TransportEvent::jump });
                }
                input.insert (input.end(), body.begin(), body.end());
            }

            auto settings = transparentSettings();
            settings.compAmount = 1.0f;
            settings.levelTracking = true;

            const auto continuous = render (input, settings), wrapped = render (input, settings, wraps), jumped = render (input, settings, jumps);

            // Mean over passes 3 to 5 of the difference between the first 4 s of each pass and the same part of the run
            // that never saw an event.
            const auto introDifference = [&] (const Render& r)
            {
                double total = 0.0;
                for (int pass = 2; pass < 5; ++pass)
                {
                    const int from = pass * loop + (int) (0.5 * kRate) + continuous.latency;
                    const Samples a (continuous.output.begin() + from, continuous.output.begin() + pass * loop + intro + continuous.latency);
                    const Samples b (r.output.begin() + from, r.output.begin() + pass * loop + intro + continuous.latency);
                    total += rmsDb (b) - rmsDb (a);
                }
                return total / 3.0;
            };

            const double wrapDifference = introDifference (wrapped), jumpDifference = introDifference (jumped);
            check (std::abs (wrapDifference) <= 0.3, "after a loop wrap the quiet start of the loop sounds as it does when playback never wrapped ("
                                                         + std::to_string (wrapDifference) + " dB difference)");
            check (std::abs (jumpDifference) >= 1.0, "after a jump the quiet start is treated as a new program and differs by "
                                                         + std::to_string (jumpDifference) + " dB");
        }
    }
}
