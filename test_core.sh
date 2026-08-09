#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$(mktemp -d /tmp/navalha-juce-core.XXXXXX)"
trap 'rm -rf -- "$BUILD_DIR"' EXIT
SANITIZER_FLAGS=""
if [ "${NAVALHA_SANITIZE:-0}" = "1" ]; then
  SANITIZER_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
fi

c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread \
  $SANITIZER_FLAGS \
  -I"$SCRIPT_DIR/src" \
  "$SCRIPT_DIR/src/core/AlbumProject.cpp" \
  "$SCRIPT_DIR/src/core/AssistedRng.cpp" \
  "$SCRIPT_DIR/src/core/AssistedPerformer.cpp" \
  "$SCRIPT_DIR/src/core/AudioComparison.cpp" \
  "$SCRIPT_DIR/src/core/AudioEngine.cpp" \
  "$SCRIPT_DIR/src/core/ControlTrace.cpp" \
  "$SCRIPT_DIR/src/core/FragmentGesture.cpp" \
  "$SCRIPT_DIR/src/core/FormDirector.cpp" \
  "$SCRIPT_DIR/src/core/HeritagePitch.cpp" \
  "$SCRIPT_DIR/src/core/Json.cpp" \
  "$SCRIPT_DIR/src/core/LookaheadLimiter.cpp" \
  "$SCRIPT_DIR/src/core/MasteringAnalysis.cpp" \
  "$SCRIPT_DIR/src/core/MasteringAlbum.cpp" \
  "$SCRIPT_DIR/src/core/MasteringAlbumManifest.cpp" \
  "$SCRIPT_DIR/src/core/MasteringProcessor.cpp" \
  "$SCRIPT_DIR/src/core/MasteringRecipe.cpp" \
  "$SCRIPT_DIR/src/core/OfflineRenderer.cpp" \
  "$SCRIPT_DIR/src/core/OutputStage.cpp" \
  "$SCRIPT_DIR/src/core/PatternTransform.cpp" \
  "$SCRIPT_DIR/src/core/PortablePath.cpp" \
  "$SCRIPT_DIR/src/core/PortableArchive.cpp" \
  "$SCRIPT_DIR/src/core/PortableProject.cpp" \
  "$SCRIPT_DIR/src/core/ProjectState.cpp" \
  "$SCRIPT_DIR/src/core/ProjectJson.cpp" \
  "$SCRIPT_DIR/src/core/RecordingWriterService.cpp" \
  "$SCRIPT_DIR/src/core/Sequencer.cpp" \
  "$SCRIPT_DIR/src/core/SessionModel.cpp" \
  "$SCRIPT_DIR/src/core/SlicePlayer.cpp" \
  "$SCRIPT_DIR/src/core/StereoMixer.cpp" \
  "$SCRIPT_DIR/src/core/TakeCatalog.cpp" \
  "$SCRIPT_DIR/src/core/TruePeakDetector.cpp" \
  "$SCRIPT_DIR/src/core/WavStreamWriter.cpp" \
  "$SCRIPT_DIR/src/core/WavMemoryReader.cpp" \
  "$SCRIPT_DIR/src/core/WavMetadataRewriter.cpp" \
  "$SCRIPT_DIR/src/core/WaveformPeaks.cpp" \
  "$SCRIPT_DIR/src/validation/TruePeakFixtures.cpp" \
  "$SCRIPT_DIR/tests/SessionModelTests.cpp" \
  -o "$BUILD_DIR/navalha_core_tests"

"$BUILD_DIR/navalha_core_tests"
