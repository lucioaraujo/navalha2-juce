#!/bin/sh
set -eu

navalha_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# .local-build/juce-app (15/11/2026, NAVALHA_BUILD_DIR do build_local.sh): a
# juce-app-native ficou presa ao caminho de antes da mudança de lugar do
# projeto e só serve de reserva.
# .local-build/juce-app-juce903 (15/11/2026): o build no JUCE 9.0.3 da família.
navalha_binary="$navalha_dir/.local-build/juce-app-juce903/src/app/Navalha2Juce_artefacts/Debug/Navalha 2"
[ -x "$navalha_binary" ] || navalha_binary="$navalha_dir/.local-build/juce-app/src/app/Navalha2Juce_artefacts/Debug/Navalha 2"
[ -x "$navalha_binary" ] || navalha_binary="$navalha_dir/.local-build/juce-app-native/src/app/Navalha2Juce_artefacts/Debug/Navalha 2"

if [ ! -x "$navalha_binary" ]; then
    printf '%s\n' \
        "Navalha 2 ainda não foi compilado." \
        "Execute: $navalha_dir/build_local.sh" >&2
    exit 1
fi

exec "$navalha_binary" "$@"
