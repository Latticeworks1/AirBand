# Tuning the AirBand constants

## Status of the constants

Every threshold, time constant and range in the DSP was carried over from convention and none was swept or ablated against reference material; the source comments in VocalGate.h, VocalCompressor.h, VocalLimiter.h and AirBand.cpp say so. This document records what the current code does, measured on the production stages at v1.3.2, what the primary sources say about the corresponding quantities in comparable designs, and the order in which the constants should be revisited. The two probes in docs/tuning reproduce the measurements: StageCurvesProbe.cpp prints static and dynamic curves for each stage, and LowLevelResponseProbe.cpp prints the low-level frequency response of the air stages. Both are built by compiling against libairband_core.a and the Harness, TestSignals and TestMetrics objects of the AirBand_Tests target, and their outputs at v1.3.2 sit beside them.

## The air path and the dry path are not time aligned

AirBand::process passes the side path through a two-times oversampler built on a polyphase IIR half-band filter, whose reported latency is 3.137 samples at 44.1 kHz, while AirBandDSP sums that path with the dry signal without delaying the dry signal. The sum is therefore a comb filter. With Mid Air at 15 dB and blend at its 20 percent default, the response at low level is +4.7 dB at 4 kHz, -6.7 dB at 7 kHz, -16.6 dB at 8 kHz, +5.4 dB at 12 kHz and -8.5 dB at 16 kHz, and High Air at 15 dB gives +2.2 dB at 6 kHz, -8.0 dB at 10 kHz and +5.3 dB at 14 kHz. A delay of 3.137 samples is a half period at 7.03 kHz, and the phase lead of the 3 kHz high-pass moves the cancellation to about 8 kHz, which agrees with the measurement. Because this ripple exceeds the effect of any level constant, thresholds cannot be judged by ear until the two paths are aligned.

## Ranges of the air controls

The Mid Air and High Air controls are labelled in decibels, but the boost a listener obtains depends on the blend. A knob value of 15 dB yields 3.2 dB at 5 kHz at 20 percent blend and 13.4 dB at 100 percent blend; a knob value of 10 dB yields 1.2 dB at 20 percent. The Dolby A encoder that the effect is modelled on has four bands at 80 Hz low-pass, 80 Hz to 3 kHz, 3 kHz high-pass and 9 kHz high-pass with 12 dB per octave slopes, a compander threshold of -40 dB, a ratio of 2:1 for a gain change of 10 dB, and about 10 dB of noise reduction rising to 15 dB at 15 kHz because the two high-pass contributions stack (Wikipedia, Dolby noise-reduction system). A search summary also gave 5 dB for the 9 kHz band, which the fetched page does not state.

## The air law threshold

The boost collapses as the band envelope approaches a threshold of -6 dBFS, with a quadratic knee in linear amplitude. At low level the boost is within 0.2 dB of its maximum up to a band level of -54 dBFS, falls to about half at -20 dBFS and is gone at -6 dBFS. The Dolby A threshold is stated relative to an unspecified reference level; if that level is -18 dBFS the transition lies between about -58 and -38 dBFS, which is 20 to 30 dB below the AirBand knee. Whether ordinary vocal high-band content sits on the plateau of the AirBand law, where the boost is static, depends on the distribution of band levels in real vocals and is measured in the results section below.

## Gate

The gate threshold is a fixed -40 dBFS on a near-peak envelope. At full amount a tone is reduced by 0.3 dB at -40 dBFS, 4.0 dB at -45, 7.8 dB at -50 and by the 18 dB cap from -64 dBFS downward. The 200 ms release is a property of the level detector, so after a phrase at -20 dBFS ends into a tail at -50 dBFS no attenuation is applied for about 400 ms and the full reduction arrives after about 900 ms. A 60 Hz rumble at -45 dBFS reduces the attenuation applied to -50 dBFS voice content from 7.8 dB to 1.8 dB, because the detector has no side-chain filter. Mix-engineering guidance for vocal gates gives attack times of 1 to 3 ms, hold times of 20 to 100 ms and release times of 50 to 120 ms (musicguymixing.com), and FabFilter Pro-G exposes threshold, ratio and range as its primary controls. Side-chain filters act only on the detection signal (Audio Masterclass).

## Compressor, de-esser and limiter

The compressor has a hard knee at -18 dBFS on a near-peak follower, a ratio of up to 4:1 and a flat makeup gain of 6 dB at full amount that applies to all material below threshold, so a quiet signal gains 6.0 dB and a signal at 0 dBFS loses 6.9 dB. Giannoulis, Massberg and Reiss recommend feed-forward designs with log-domain detection and a variable knee; the full text was not retrievable and only its abstract-level description was read. The sibilance ratio of the de-esser, the level of a 6.5 kHz band-pass divided by the level of the 9 kHz high-pass band, measures 0.57 for white noise, 2.07 for noise between 3 and 6 kHz, 1.39 for noise between 5 and 8 kHz and 0.34 for noise between 10 and 16 kHz, and the code clamps it at 1, so sibilant content saturates it. The control reduces only the boost of the 9 kHz band, while the 3 kHz band, which covers male sibilance at 3 to 6 kHz and female sibilance at 5 to 8 kHz, has no de-esser. The limiter's 5 ms lookahead and 60 ms release are conventional and no evidence argues for changing them.

## Order of work

The air path is aligned first, with the golden vectors re-recorded because the change alters the sound. The distribution of band levels is then measured on real vocal material. The thresholds are chosen from those distributions and checked with level-matched renders. The gate is restructured so that the detector is fast, the gain has separate attack, hold and release, the threshold is a parameter and the detector carries a high-pass.

## Results
