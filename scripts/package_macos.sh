#!/usr/bin/env bash
# Builds a Release, universal (arm64 + x86_64) build of AirBand into a clean
# tree and zips the VST3/AU bundles for distribution. Does not touch the
# user's installed dev build in ~/Library/Audio/Plug-Ins.
set -euo pipefail

cd "$(dirname "$0")/.."

VERSION=$(grep -m1 'project(AirBand VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')
BUILD_DIR="build-release"
DIST_DIR="dist"

echo "Configuring release build (v${VERSION}, universal binary)..."
cmake -B "$BUILD_DIR" -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DAIRBAND_COPY_AFTER_BUILD=OFF

echo "Building VST3 and AU..."
cmake --build "$BUILD_DIR" --target AirBand_VST3 --target AirBand_AU -j "$(sysctl -n hw.ncpu)"

ARTEFACTS="$BUILD_DIR/AirBand_artefacts/Release"

# The VST3 post-build step writes Resources/moduleinfo.json after the linker's
# signature is applied, and the AU bundle only carries the linker's
# executable-level signature. Re-seal both bundles ad hoc so the whole bundle
# verifies, and refuse to package if it does not.
echo "Sealing bundles (ad-hoc)..."
for bundle in "$ARTEFACTS/VST3/AirBand.vst3" "$ARTEFACTS/AU/AirBand.component"; do
    codesign --force --deep --sign - --timestamp=none "$bundle"
    codesign --verify --deep --strict "$bundle"
done

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/AirBand-${VERSION}"

cp -R "$ARTEFACTS/VST3/AirBand.vst3" "$DIST_DIR/AirBand-${VERSION}/"
cp -R "$ARTEFACTS/AU/AirBand.component" "$DIST_DIR/AirBand-${VERSION}/"
cp README.md "$DIST_DIR/AirBand-${VERSION}/" 2>/dev/null || true

echo "Verifying architectures..."
lipo -info "$DIST_DIR/AirBand-${VERSION}/AirBand.vst3/Contents/MacOS/AirBand"
lipo -info "$DIST_DIR/AirBand-${VERSION}/AirBand.component/Contents/MacOS/AirBand"

pushd "$DIST_DIR" > /dev/null
zip -r -q "AirBand-macOS.zip" "AirBand-${VERSION}"
popd > /dev/null

echo
echo "Packaged: ${DIST_DIR}/AirBand-macOS.zip"
echo "Signature is ad-hoc only (no Developer ID cert on this machine)."
echo "Recipients on other Macs will need to clear the quarantine flag:"
echo "  xattr -cr \"/path/to/AirBand.vst3\" \"/path/to/AirBand.component\""
