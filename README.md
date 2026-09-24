# AirBand

AirBand is an audio effect plugin that recreates the "encoder-only" Dolby A
hack: two bands of a 1970s noise-reduction encoder, run without their
matching decoder, used purely for their side effect of dynamic treble
lift.

> [!WARNING]
> There are no signed builds on either platform. macOS downloads are ad-hoc
> signed only (no Apple Developer ID on the build machine), so Gatekeeper
> will quarantine the plugin on first launch on any Mac other than the one it
> was built on. The Windows build is unsigned too, so SmartScreen may warn on
> first run. See [Installing](#installing) to get past both. There is no
> Linux build.

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

- **Mid Air** — boost applied to quiet content in the ~3 kHz-and-up band, 0-15 dB.
- **High Air** — boost applied to quiet content in the ~9 kHz-and-up band, 0-15 dB.
- **De-Ess** — how much the High Air boost pulls back when that band's energy
  is concentrated in the vocal sibilant range rather than spread across it,
  0-100%. Keeps aggressive High Air settings from turning "S" sounds into
  their own boosted transient.
- **Air Blend** — how much of the processed bands is summed back with the dry
  signal, 0-100%. The original hardware mod ran in the 16-22% range.
- **Output** — output trim, ±12 dB.

## Installing

All downloads are under
[Releases](https://github.com/Latticeworks1/AirBand/releases).

**macOS**

- **`AirBand-<version>.pkg`** — a standard installer. Double-click it and it
  places the VST3 and AU into `/Library/Audio/Plug-Ins/`, the same location
  most commercial plugins use.
- **`AirBand-<version>-macOS.zip`** — manual install. Unzip, then copy
  `AirBand.vst3` into `~/Library/Audio/Plug-Ins/VST3/` and
  `AirBand.component` into `~/Library/Audio/Plug-Ins/Components/`.

Neither is notarized, so Gatekeeper will block the first launch. Right-click
the `.pkg` and choose **Open**, or clear the quarantine flag on the zip
contents before scanning:

```bash
xattr -cr "/path/to/AirBand.vst3" "/path/to/AirBand.component"
```

**Windows**

- **`AirBand-<version>-windows.zip`** — unzip, then copy `AirBand.vst3`
  into `C:\Program Files\Common Files\VST3\`.

It isn't signed, so SmartScreen may show an "unrecognized app" prompt the
first time your DAW loads it — choose **More info → Run anyway**.

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

## License

AirBand's own source is released under the [MIT license](LICENSE).

AirBand links against [JUCE](https://juce.com/), which is dual-licensed.
Builds produced from this repository use JUCE under its AGPLv3 terms
(no commercial JUCE license is held for this project), which means any
distributed build's complete corresponding source must remain available —
satisfied here by this repository being public. Anyone building or
redistributing AirBand under a commercial JUCE license is not bound by
that requirement.
