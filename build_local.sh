#!/bin/sh
set -eu

juce_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
workspace_dir=$(dirname -- "$juce_dir")
pd_dir=${NAVALHA_PD_PATH:-"$workspace_dir/NAVALHA2_PD"}
dependency_dir=${NAVALHA_DEPENDENCY_DIR:-"$pd_dir/.local-deps"}
build_dir=${NAVALHA_BUILD_DIR:-"$juce_dir/.local-build/juce-app-native"}
cmake_bin="$dependency_dir/cmake-3.28.3/bin/cmake"
ctest_bin="$dependency_dir/cmake-3.28.3/bin/ctest"
juce_sdk="$dependency_dir/JUCE-8.0.13"
sysroot="$dependency_dir/sysroot"
navalha_jobs=${NAVALHA_JOBS:-2}

if [ ! -x "$cmake_bin" ] || [ ! -f "$juce_sdk/CMakeLists.txt" ]; then
    echo "Dependências locais ausentes em $dependency_dir" >&2
    exit 1
fi

export CPATH="$sysroot/usr/include${CPATH:+:$CPATH}"
export LIBRARY_PATH="$sysroot/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}"
export PKG_CONFIG_PATH="$sysroot/usr/lib/x86_64-linux-gnu/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

"$cmake_bin" -S "$juce_dir" -B "$build_dir" \
    -DNAVALHA_JUCE_PATH="$juce_sdk" \
    -DNAVALHA_PD_PATH="$pd_dir" \
    -DNAVALHA_BUILD_JUCE_APP=ON \
    -DNAVALHA_ENABLE_WEBVIEW=OFF \
    -DNAVALHA_BUILD_TESTS=ON \
    -DCMAKE_BUILD_TYPE=Debug
"$cmake_bin" --build "$build_dir" --parallel "$navalha_jobs"
"$ctest_bin" --test-dir "$build_dir" --output-on-failure
