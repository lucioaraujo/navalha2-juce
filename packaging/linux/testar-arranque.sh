#!/bin/sh
# Launch test: starts the app on a virtual screen (Xvfb), with no sound card,
# and checks that it is still running after N seconds AND that its window
# exists. What CI runs in containers of several distributions (RASGO
# multiplatform standard). Catches the defect that costs a user the most:
# the app that does not even open (missing library, too-old glibc, a .deb
# with an unresolvable dependency).
#
#   packaging/linux/testar-arranque.sh <executable-or-AppImage> <window-title> [seconds]
set -eu
EXE=$1
TITLE=$2
WAIT=${3:-20}

H=$(mktemp -d)
export HOME="$H"
export APPIMAGE_EXTRACT_AND_RUN=1

XPID=
if command -v Xvfb >/dev/null 2>&1; then
    Xvfb :77 -screen 0 1600x1000x24 -nolisten tcp >/dev/null 2>&1 &
    XPID=$!
    export DISPLAY=:77
    sleep 2
fi

"$EXE" >"$H/out.txt" 2>&1 &
PID=$!
sleep "$WAIT"

ok=1
if kill -0 "$PID" 2>/dev/null; then
    echo "still running after ${WAIT} s"
else
    wait "$PID" && st=0 || st=$?
    echo "EXITED — status $st"; ok=0
fi
if command -v xwininfo >/dev/null 2>&1; then
    if xwininfo -root -tree 2>/dev/null | grep -F -q "$TITLE"; then
        echo "window found: $TITLE"
    else
        echo "window NOT found: $TITLE"; ok=0
    fi
else
    echo "(xwininfo not installed: window not checked)"
fi
echo "--- app output (last lines) ---"; tail -n 20 "$H/out.txt" || true
kill "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true
[ -n "$XPID" ] && kill "$XPID" 2>/dev/null || true
rm -rf "$H"
[ "$ok" = 1 ] && echo "OK: $EXE" || { echo "FAILED: $EXE"; exit 1; }
