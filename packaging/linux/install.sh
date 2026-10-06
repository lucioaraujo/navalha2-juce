#!/bin/sh
# Installs this program into your home folder (~/.local), without root.
#   ./install.sh            install (or update)
#   ./install.sh --remove   remove what this script installed
# Other destination: PREFIX=/opt/x ./install.sh
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
PREFIX=${PREFIX:-"$HOME/.local"}
BIN=$(ls "$HERE/bin" | head -1)
DESKTOP=$(ls "$HERE/share/applications" | head -1)

if [ "${1:-}" = "--remove" ]; then
    rm -f "$PREFIX/bin/$BIN" "$PREFIX/share/applications/$DESKTOP"
    (cd "$HERE/share" && find icons -type f) | while read -r f; do rm -f "$PREFIX/share/$f"; done
    echo "Removido de / removed from $PREFIX."
    exit 0
fi

mkdir -p "$PREFIX/bin" "$PREFIX/share/applications"
cp "$HERE/bin/$BIN" "$PREFIX/bin/"
(cd "$HERE/share" && find icons -type f) | while read -r f; do
    mkdir -p "$PREFIX/share/$(dirname "$f")"
    cp "$HERE/share/$f" "$PREFIX/share/$f"
done
# absolute path in Exec: ~/.local/bin is not always on the menu's PATH
sed "s|^Exec=.*|Exec=\"$PREFIX/bin/$BIN\"|" "$HERE/share/applications/$DESKTOP" \
    > "$PREFIX/share/applications/$DESKTOP"
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$PREFIX/share/applications" 2>/dev/null || true
echo "Instalado em / installed to $PREFIX/bin/$BIN"
echo "(no menu de aplicativos / in the application menu)"
