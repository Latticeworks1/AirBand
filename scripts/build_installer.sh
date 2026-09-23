#!/usr/bin/env bash
# Builds a double-clickable macOS .pkg installer for AirBand, installing the
# VST3 and AU into /Library/Audio/Plug-Ins the same way commercial plugin
# installers do. Run scripts/package_macos.sh first (this reuses its Release
# build output).
set -euo pipefail

cd "$(dirname "$0")/.."

VERSION=$(grep -m1 'project(AirBand VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')
BUILD_DIR="build-release"
ARTEFACTS="$BUILD_DIR/AirBand_artefacts/Release"
PKG_ROOT="build-release/pkg-root"
PKG_OUT="dist"

if [ ! -d "$ARTEFACTS/VST3/AirBand.vst3" ] || [ ! -d "$ARTEFACTS/AU/AirBand.component" ]; then
    echo "Release artefacts not found — run scripts/package_macos.sh first." >&2
    exit 1
fi

rm -rf "$PKG_ROOT"
mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/VST3"
mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/Components"

cp -R "$ARTEFACTS/VST3/AirBand.vst3" "$PKG_ROOT/Library/Audio/Plug-Ins/VST3/"
cp -R "$ARTEFACTS/AU/AirBand.component" "$PKG_ROOT/Library/Audio/Plug-Ins/Components/"

mkdir -p "$PKG_OUT"
COMPONENT_PKG="$BUILD_DIR/AirBandComponent.pkg"

pkgbuild \
    --root "$PKG_ROOT" \
    --identifier "com.airband.airband.pkg" \
    --version "$VERSION" \
    --install-location "/" \
    "$COMPONENT_PKG"

DISTRIBUTION_XML="$BUILD_DIR/distribution.xml"
cat > "$DISTRIBUTION_XML" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>AirBand ${VERSION}</title>
    <organization>com.airband</organization>
    <domains enable_localSystem="true"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <choices-outline>
        <line choice="default">
            <line choice="com.airband.airband.pkg"/>
        </line>
    </choices-outline>
    <choice id="default"/>
    <choice id="com.airband.airband.pkg" visible="false">
        <pkg-ref id="com.airband.airband.pkg"/>
    </choice>
    <pkg-ref id="com.airband.airband.pkg" version="${VERSION}" onConclusion="none">AirBandComponent.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild \
    --distribution "$DISTRIBUTION_XML" \
    --package-path "$BUILD_DIR" \
    "$PKG_OUT/AirBand-${VERSION}.pkg"

echo
echo "Installer built: ${PKG_OUT}/AirBand-${VERSION}.pkg"
echo "Unsigned (no Developer ID Installer certificate on this machine) — macOS"
echo "will show an 'unidentified developer' warning; right-click > Open to run it,"
echo "or System Settings > Privacy & Security > Open Anyway after the first block."
