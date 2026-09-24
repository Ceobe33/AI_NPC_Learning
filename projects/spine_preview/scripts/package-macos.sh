#!/usr/bin/env bash
#
# Builds a self-contained macOS application other machines can run.
#
#   ./scripts/package-macos.sh                 # arm64 (Apple silicon)
#   SPINE_STUDIO_ARCHS="arm64;x86_64" ./scripts/package-macos.sh
#
# Produces release/SpineAnimationStudio.app plus a .zip next to it.
#
# The app is ad-hoc signed, which is enough to make macOS accept it locally but
# not enough for Gatekeeper to trust a copy that arrived from somewhere else.
# Without an Apple Developer ID certificate the recipient has to open it once
# via right-click -> Open, or run:  xattr -cr "Spine Animation Studio.app"
# The generated release/README.txt says the same thing in the form that ships.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-release"
RELEASE_DIR="${PROJECT_DIR}/release"
APP_BUNDLE="${RELEASE_DIR}/SpineAnimationStudio.app"
BINARY_NAME="SpineAnimationStudio"
IDENTIFIER="com.spinestudio.animation-studio"
VERSION="${SPINE_STUDIO_VERSION:-1.0.0}"

# A Mac built today is arm64, but the point of packaging is handing the app to
# someone else, who may well be on Intel. Both slices in one file costs a
# second build pass and removes that question entirely.
ARCHS="${SPINE_STUDIO_ARCHS:-arm64;x86_64}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this script builds a macOS application bundle" >&2
    exit 1
fi

CMAKE_BIN="${CMAKE_BIN:-cmake}"
if ! command -v "${CMAKE_BIN}" >/dev/null 2>&1; then
    CMAKE_BIN="/usr/local/bin/cmake"
fi

echo "==> configuring (${ARCHS})"

# The first configure in a fresh build tree has to fetch GLFW and Dear ImGui;
# after that they are already on disk and staying offline keeps the packaging
# reproducible.
FETCH=OFF
if [[ -d "${BUILD_DIR}/_deps" ]]; then
    FETCH=ON
fi

"${CMAKE_BIN}" -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="${ARCHS}" \
    -DFETCHCONTENT_FULLY_DISCONNECTED="${FETCH}"

echo "==> building"

"${CMAKE_BIN}" --build "${BUILD_DIR}" --parallel

BINARY="${BUILD_DIR}/${BINARY_NAME}"

if [[ ! -f "${BINARY}" ]]; then
    echo "error: expected ${BINARY} after the build" >&2
    exit 1
fi

echo "==> assembling ${APP_BUNDLE}"

rm -rf "${APP_BUNDLE}"
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"

cp "${BINARY}" "${APP_BUNDLE}/Contents/MacOS/${BINARY_NAME}"

cat > "${APP_BUNDLE}/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>${BINARY_NAME}</string>
    <key>CFBundleDisplayName</key>
    <string>Spine Animation Studio</string>
    <key>CFBundleIdentifier</key>
    <string>${IDENTIFIER}</string>
    <key>CFBundleExecutable</key>
    <string>${BINARY_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CFBundleSupportedPlatforms</key>
    <array><string>MacOSX</string></array>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
PLIST

echo "==> signing (ad hoc)"

# -s - is an ad-hoc signature: no identity, but the bundle gets the sealed
# structure macOS insists on before it will launch anything.
codesign --force --deep --sign - "${APP_BUNDLE}"
codesign --verify --verbose=2 "${APP_BUNDLE}" 2>&1 | tail -2

ARCH_TAG="$(echo "${ARCHS}" | tr ';' '-')"
ZIP="${RELEASE_DIR}/SpineAnimationStudio-macos-${ARCH_TAG}.zip"

echo "==> zipping ${ZIP}"

rm -f "${ZIP}"
ditto -c -k --sequesterRsrc --keepParent "${APP_BUNDLE}" "${ZIP}"

cat > "${RELEASE_DIR}/README.txt" <<README
Spine Animation Studio ${VERSION} for macOS (${ARCH_TAG})

Built from projects/spine_preview by scripts/package-macos.sh.

Running it
----------
1. Unzip and drag SpineAnimationStudio.app into /Applications.
2. First launch: right-click the app and choose Open, then confirm.
   (Or, in Terminal:  xattr -cr /Applications/SpineAnimationStudio.app)

   Why: the app is ad-hoc signed because there is no Apple Developer ID
   certificate behind it. macOS therefore marks a copy that arrived over the
   network or AirDrop as quarantined and refuses a plain double-click. Opening
   it once explicitly is the user's way of vouching for it; after that a normal
   double-click works.

Where it keeps things
---------------------
Window layout: ~/Library/Application Support/Spine Animation Studio/layout.ini
GIF exports:   wherever you choose in the save panel.

Nothing is installed outside the app bundle.
README

echo "==> done"
echo "    app: ${APP_BUNDLE}"
echo "    zip: ${ZIP}"

echo
echo "linked against (only system libraries may appear here):"
otool -L "${APP_BUNDLE}/Contents/MacOS/${BINARY_NAME}" | sed -n '2,20p'
