#!/bin/sh
set -eu

navalha_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
navalha_binary="$navalha_dir/.local-build/juce-app-native/src/app/Navalha2Juce_artefacts/Debug/Navalha 2"

if [ ! -x "$navalha_binary" ]; then
    printf '%s\n' \
        "Navalha 2 ainda não foi compilado." \
        "Execute: $navalha_dir/build_local.sh" >&2
    exit 1
fi

exec "$navalha_binary" "$@"
