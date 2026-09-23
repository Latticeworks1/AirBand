# AirBand

A two-band parallel companding effect modelled on the mid-1970s "encoder-only"
Dolby A hack: two bands of the noise-reduction encoder (bands 3 and 4, high-passed
at roughly 3 kHz and 9 kHz) run their companding action, and the result is left
encoded rather than decoded. Because encode-only companding boosts quiet
high-frequency content while leaving loud high-frequency content close to
unity, the result reads as a dynamic treble lift rather than a static shelf or
a harmonic exciter.

## Controls

- **Mid Air** — boost applied to quiet content in the ~3 kHz-and-up band, 0-15 dB.
- **High Air** — boost applied to quiet content in the ~9 kHz-and-up band, 0-15 dB.
- **Air Blend** — how much of the processed bands is summed back with the dry
  signal, 0-100%. The historical hardware mod ran in the 16-22% range.
- **Output** — output trim, ±12 dB.

## Installing

Copy the plugin bundle(s) into:

- VST3: `~/Library/Audio/Plug-Ins/VST3/`
- AU: `~/Library/Audio/Plug-Ins/Components/`

Then rescan plugins in your DAW (in FL Studio: **Options → Manage Plugins →
Find Plugins**).

This build is ad-hoc signed rather than notarized with an Apple Developer ID.
On a Mac other than the one it was built on, Gatekeeper will quarantine it on
first download. Clear the quarantine flag before scanning:

```bash
xattr -cr "/path/to/AirBand.vst3" "/path/to/AirBand.component"
```

## Building from source

Requires CMake 3.22+ and Xcode command line tools.

```bash
cmake -B build -G "Unix Makefiles"
cmake --build build --target AirBand_VST3 --target AirBand_AU -j 8
```

To produce a distributable, universal-binary Release build:

```bash
scripts/package_macos.sh
```
