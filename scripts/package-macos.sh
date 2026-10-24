#!/usr/bin/env bash
# Builds dist/PDF-Handler-<version>-macOS.dmg from a configured+built CMake build dir.
#   scripts/package-macos.sh [build-dir]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(cd "${1:-$ROOT/build/release}" && pwd)"
VERSION="$(sed -n 's/^ *VERSION \([0-9.]*\)$/\1/p' "$ROOT/CMakeLists.txt" | head -1)"
STAGE="$BUILD/dmg"
DIST="$ROOT/dist"

rm -rf "$STAGE"
mkdir -p "$STAGE" "$DIST"
cmake --install "$BUILD" --prefix "$STAGE"   # runs macdeployqt via Qt's deploy script
rm -rf "$STAGE/bin" "$STAGE/lib" "$STAGE/include" "$STAGE/share"  # only the .app goes in
ln -s /Applications "$STAGE/Applications"

# Ad-hoc signature so Gatekeeper reports "unidentified developer" instead of "damaged".
codesign --force --deep --sign - "$STAGE/PDF Handler.app"

hdiutil create -volname "PDF Handler" -srcfolder "$STAGE" -ov -format UDZO \
    "$DIST/PDF-Handler-$VERSION-macOS.dmg"
echo "Built $DIST/PDF-Handler-$VERSION-macOS.dmg"
