#!/usr/bin/env bash
# Builds the AppImage and the .tar.gz from a finished build — the generic
# version of the RASGO multiplatform standard
# (RASGO_DOCUMENTATION/PADRAO_DISTRIBUICAO_MULTIPLATAFORMA.md).
#
#   packaging/linux/empacotar.sh <build-dir> <output-dir> <package-prefix>
#
# Both packages come from the SAME `cmake --install` tree the .deb uses, so
# there is no second file list to keep in sync. Binary, .desktop and icon
# are discovered in that tree: exactly one file in usr/bin, one .desktop in
# usr/share/applications, and the icon named by its Icon= line.
#
# - AppImage: one file, non-system libraries bundled by linuxdeploy
#   (pinned by version and SHA-256 — third-party tool in the release path).
# - .tar.gz: the plain binary + install.sh (installs into ~/.local, no root)
#   + README.txt.
# Built by CI on Ubuntu 22.04 (glibc 2.35): runs on 2022+ distributions.
set -euo pipefail

BUILD=${1:?usage: empacotar.sh <build-dir> <output-dir> <package-prefix>}
OUT=${2:?usage: empacotar.sh <build-dir> <output-dir> <package-prefix>}
PREFIX=${3:?usage: empacotar.sh <build-dir> <output-dir> <package-prefix>}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)

LINUXDEPLOY_VERSION=1-alpha-20251107-1
LINUXDEPLOY_SHA256=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d

VERSION=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$BUILD/CMakeCache.txt")
[ -n "$VERSION" ] || { echo "no version in $BUILD/CMakeCache.txt" >&2; exit 1; }
ARCH=$(uname -m)
NAME="${PREFIX}-${VERSION}-linux-${ARCH}"

mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# ---- installed tree (base of both packages) ------------------------------
cmake --install "$BUILD" --config Release --prefix "$WORK/AppDir/usr"
mapfile -t BINS < <(find "$WORK/AppDir/usr/bin" -maxdepth 1 -type f -perm -u+x)
[ "${#BINS[@]}" -eq 1 ] || { echo "expected exactly one executable in usr/bin, found ${#BINS[@]}" >&2; exit 1; }
BIN=${BINS[0]}
strip --strip-unneeded "$BIN"
DESKTOP=$(find "$WORK/AppDir/usr/share/applications" -maxdepth 1 -name '*.desktop' | head -1)
[ -n "$DESKTOP" ] || { echo "no .desktop installed" >&2; exit 1; }
ICON_NAME=$(sed -n 's/^Icon=//p' "$DESKTOP" | head -1)
# the largest PNG with that name; the SVG if there is no PNG
ICON=$(find "$WORK/AppDir/usr/share/icons" -type f -name "$ICON_NAME.png" 2>/dev/null | sort -V | tail -1)
[ -n "$ICON" ] || ICON=$(find "$WORK/AppDir/usr/share/icons" -type f -name "$ICON_NAME.svg" 2>/dev/null | head -1)
[ -n "$ICON" ] || { echo "icon '$ICON_NAME' not installed" >&2; exit 1; }

# ---- .tar.gz -------------------------------------------------------------
T="$WORK/$NAME"
mkdir -p "$T"
cp -a "$WORK/AppDir/usr/bin" "$WORK/AppDir/usr/share" "$T/"
for f in LICENSE COPYING; do [ -f "$ROOT/$f" ] && cp "$ROOT/$f" "$T/"; done
cp "$HERE/install.sh" "$T/"
cp "$HERE/README-tarball.txt" "$T/README.txt"
chmod +x "$T/install.sh"
tar -C "$WORK" --owner=0 --group=0 -czf "$OUT/$NAME.tar.gz" "$NAME"

# ---- AppImage ------------------------------------------------------------
LD="$WORK/linuxdeploy-${ARCH}.AppImage"
if [ -n "${LINUXDEPLOY:-}" ]; then
    cp "$LINUXDEPLOY" "$LD"
else
    curl -fsSL -o "$LD" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_VERSION}/linuxdeploy-${ARCH}.AppImage"
fi
echo "${LINUXDEPLOY_SHA256}  $LD" | sha256sum -c -
chmod +x "$LD"
export APPIMAGE_EXTRACT_AND_RUN=1   # no FUSE in containers/CI
export LDAI_OUTPUT="$OUT/$NAME.AppImage"
export ARCH
(cd "$WORK" && "$LD" --appdir AppDir --executable "$BIN" --desktop-file "$DESKTOP" \
    --icon-file "$ICON" --output appimage)
chmod +x "$OUT/$NAME.AppImage"
ls -l "$OUT/$NAME.tar.gz" "$OUT/$NAME.AppImage"
