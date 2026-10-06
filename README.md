# AirBand

AirBand is an audio effect plugin that recreates the "encoder-only" Dolby A
hack: two bands of a 1970s noise-reduction encoder, run without their
matching decoder, used purely for their side effect of dynamic treble
lift.

<p align="center">
  <a href="https://github.com/Latticeworks1/AirBand/releases/latest/download/AirBand-windows-setup.exe"><img src="https://img.shields.io/badge/Download%20for%20Windows%20(installer)-0078D6?style=for-the-badge" alt="Download for Windows (installer)"></a>
  <a href="https://github.com/Latticeworks1/AirBand/releases/latest/download/AirBand-windows.zip"><img src="https://img.shields.io/badge/Download%20for%20Windows%20(zip)-0078D6?style=for-the-badge" alt="Download for Windows (zip)"></a>
  <a href="https://github.com/Latticeworks1/AirBand/releases/latest/download/AirBand-macOS.pkg"><img src="https://img.shields.io/badge/Download%20for%20macOS%20(installer)-555555?style=for-the-badge" alt="Download for macOS (installer)"></a>
  <a href="https://github.com/Latticeworks1/AirBand/releases/latest/download/AirBand-macOS.zip"><img src="https://img.shields.io/badge/Download%20for%20macOS%20(zip)-555555?style=for-the-badge" alt="Download for macOS (zip)"></a>
</p>

## How it works

Two of the four bands in a Dolby A encoder are high-pass filtered (roughly
3 kHz and 9 kHz) and run through the encoder's companding action, which
boosts quiet high-frequency content while leaving loud high-frequency
content close to unity gain. Normally that companded signal is summed with
a matching decoder to cancel the boost back out. Leaving it undecoded and
summing it back with the dry signal instead produces a treble lift that
tracks program level rather than a static shelf or a harmonic exciter.
AirBand implements that same two-band, level-dependent companding directly,
without emulating the rest of the Dolby A signal path.

## Controls

Signal flows through the controls in this order:

- **Gate** — attenuates quiet breath and mouth noise before it reaches the
  compressor's makeup gain, 0-100%. At 0% it is exactly transparent;
  increasing it raises both the downward expansion ratio (up to 4:1) and
  the maximum attenuation (up to -18 dB) applied below the gate threshold.
- **Comp** — a broadband compressor applied to the dry signal before the air
  bands, so the vocal is levelled before the treble lift is added on top,
  0-100%. At 0% it is exactly transparent; increasing it raises both the
  compression ratio (up to 4:1) and an automatic makeup gain (up to +6 dB)
  together.
- **Mid Air** — boost applied to quiet content in the ~3 kHz-and-up band, 0-15 dB.
- **High Air** — boost applied to quiet content in the ~9 kHz-and-up band, 0-15 dB.
- **De-Ess** — how much the High Air boost pulls back when that band's energy
  is concentrated in the vocal sibilant range rather than spread across it,
  0-100%. Keeps aggressive High Air settings from turning "S" sounds into
  their own boosted transient.
- **Air Blend** — how much of the processed bands is summed back with the dry
  signal, 0-100%. The original hardware mod ran in the 16-22% range.
- **Output** — output trim, ±12 dB.
- **Limiter** — a lookahead peak limiter, the final stage, catching whatever
  the stages above stack up to. Sets the output ceiling, -12 to 0 dB
  (default 0 dB: a pure safety net that only engages if something upstream
  would otherwise clip). Because it looks ahead, AirBand reports a small
  amount of latency (5 ms) to the host for plugin delay compensation.

## Installing

All downloads are under
[Releases](https://github.com/Latticeworks1/AirBand/releases).

**macOS**

- **`AirBand-macOS.pkg`** — a standard installer. Double-click it and it
  places the VST3 and AU into `/Library/Audio/Plug-Ins/`, the same location
  most commercial plugins use.
- **`AirBand-macOS.zip`** — manual install. Unzip, then copy
  `AirBand.vst3` into `~/Library/Audio/Plug-Ins/VST3/` and
  `AirBand.component` into `~/Library/Audio/Plug-Ins/Components/`.

Neither is notarized, so Gatekeeper will block the first launch. Right-click
the `.pkg` and choose **Open**, or clear the quarantine flag on the zip
contents before scanning:

```bash
xattr -cr "/path/to/AirBand.vst3" "/path/to/AirBand.component"
```

**Windows**

- **`AirBand-windows-setup.exe`** — an installer wizard. Run it and it
  places `AirBand.vst3` into `C:\Program Files\Common Files\VST3\`, and it
  can be removed later from Settings, Apps. The plugin is a single 64-bit
  VST3 with the C++ runtime built in, so nothing else needs to be installed.
- **`AirBand-windows.zip`** — manual install. Unzip, then copy `AirBand.vst3`
  into `C:\Program Files\Common Files\VST3\`.

Neither Windows file is signed, so SmartScreen may show an "unrecognized app"
prompt when you run the installer or first load the plugin — choose **More
info → Run anyway**.

After installing on either platform, rescan plugins in your DAW (in FL
Studio: **Options → Manage Plugins → Find Plugins**).

## Build from source

Requirements: CMake 3.22+, a C++20 compiler (Xcode command line tools on
macOS), and internet access on first configure (JUCE is fetched via CMake's
`FetchContent`).

```bash
git clone https://github.com/Latticeworks1/AirBand
cd AirBand
cmake -B build -G "Unix Makefiles"
cmake --build build --target AirBand_VST3 --target AirBand_AU -j 8
```

By default this also copies the built plugin straight into
`~/Library/Audio/Plug-Ins/` for local testing (`AIRBAND_COPY_AFTER_BUILD`
in `CMakeLists.txt`).

To produce a distributable, universal-binary (arm64 + x86_64) Release
build instead:

```bash
scripts/package_macos.sh      # zip of the VST3/AU bundles
scripts/build_installer.sh    # .pkg installer, run after package_macos.sh
```

## Testing

`AirBand_Tests` runs the same `airband_core` library that the plugin links, with no
plugin host involved. Stimuli (`TestSignals`), measurements (`TestMetrics`) and
specifications are separate files. The specifications cover linear behaviour
(unity pass-through, impulse response, reported latency), the air bands, the
gate, compressor and limiter, and safety (extreme input, oversized blocks,
channel counts). Output must also be the same for every way of cutting the
input into blocks (1, 64, 128, 512, 1024, 511+1+512, 257+255+512 samples): bit for
bit where JUCE's snap-to-zero is a no-op (arm64), and within 1e-7 on Intel CPUs,
where JUCE zeroes oversampler filter states below 1e-8 once per processing call
and so depends slightly on where the block cuts fall. Recorded golden vectors in
`Tests/GoldenVectors.h` pin the exact output for twelve fixed stimuli. The golden
hashes were recorded on macOS arm64 in a release build and are compared bit for bit
there; other platforms compare RMS to 5e-4 dB and peak to 1e-6 relative, tolerances
set at about ten times the drift measured with fused multiply-add disabled. The
largest drift measured on Windows (MSVC) is 4.9e-5 dB RMS and 1.2e-7 relative peak.

```bash
cmake -S . -B build && cmake --build build --target AirBand_Tests
./build/AirBand_Tests_artefacts/Release/AirBand_Tests
./build/AirBand_HostTests_artefacts/Release/AirBand_HostTests   # parameter defaults and host-unit conversion
scripts/check_realtime.sh     # same tests under clang RealtimeSanitizer (needs Homebrew LLVM)
```

A change that is meant to alter the sound must re-record the vectors with
`AirBand_Tests --record` and say so in the commit; every other change must leave
them untouched.

## License

AirBand's own source is released under the [MIT license](LICENSE).

AirBand links against [JUCE](https://juce.com/), which is dual-licensed.
Builds produced from this repository use JUCE under its AGPLv3 terms
(no commercial JUCE license is held for this project), which means any
distributed build's complete corresponding source must remain available —
satisfied here by this repository being public. Anyone building or
redistributing AirBand under a commercial JUCE license is not bound by
that requirement.
