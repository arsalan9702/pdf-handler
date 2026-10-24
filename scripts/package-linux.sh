#!/usr/bin/env bash
# Builds dist/PDF-Handler-<version>-x86_64.AppImage from a configured+built CMake build dir.
#   scripts/package-linux.sh [build-dir] [qmake]
# Needs network access the first time to fetch linuxdeploy (MIT) and its Qt plugin.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(cd "${1:-$ROOT/build/release}" && pwd)"
QMAKE_BIN="${2:-$(command -v qmake6 || command -v qmake)}"
TOOLS="$ROOT/build/tools"
APPDIR="$BUILD/AppDir"
DIST="$ROOT/dist"
VERSION="$(sed -n 's/^ *VERSION \([0-9.]*\)$/\1/p' "$ROOT/CMakeLists.txt" | head -1)"

mkdir -p "$TOOLS" "$DIST"
for tool in linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage; do
    if [ ! -x "$TOOLS/$tool" ]; then
        repo="linuxdeploy/${tool%-x86_64.AppImage}"
        curl -fsSL -o "$TOOLS/$tool" "https://github.com/$repo/releases/download/continuous/$tool"
        chmod +x "$TOOLS/$tool"
    fi
done

rm -rf "$APPDIR"
cmake --install "$BUILD" --prefix "$APPDIR/usr"

export APPIMAGE_EXTRACT_AND_RUN=1          # no FUSE needed for the tools themselves
export NO_STRIP=1                          # bundled strip is too old for newer toolchains
export QMAKE="$QMAKE_BIN"
export QML_SOURCES_PATHS="$ROOT/app/qml"   # so the plugin bundles every QML import we use
# Wayland (when this Qt has it) plus offscreen for headless runs and the screenshot hook.
PLUGINS_DIR="$("$QMAKE_BIN" -query QT_INSTALL_PLUGINS)"
platforms="libqoffscreen.so"
modules="svg"
for wl in libqwayland.so libqwayland-generic.so libqwayland-egl.so; do
    if [ -f "$PLUGINS_DIR/platforms/$wl" ]; then
        platforms="$platforms;$wl"
        modules="svg;waylandclient"
    fi
done
export EXTRA_PLATFORM_PLUGINS="$platforms"
export EXTRA_QT_MODULES="$modules"
export LDAI_OUTPUT="$DIST/PDF-Handler-$VERSION-x86_64.AppImage"
export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"

cd "$BUILD"
"$TOOLS/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/io.github.pdfhandler.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/io.github.pdfhandler.png" \
    --plugin qt \
    --output appimage

echo "Built $LDAI_OUTPUT"
