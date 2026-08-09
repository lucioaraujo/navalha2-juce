#pragma once

#include <cstddef>

#include "core/SlicePlayer.h"

namespace navalha
{
struct MasteringMetrics
{
    std::size_t inspectedFrames = 0;
    double peak = 0.0;
    double peakDb = -240.0;
    double rms = 0.0;
    double rmsDb = -240.0;
    double estimatedLufs = -120.691;
    double crestDb = 0.0;
    double correlation = 0.0;
    double headroomDb = 240.0;
};

// Mirrors the lightweight v0.28.1 MASTER meter. This is intentionally an
// internal estimate, not an EBU R128/ITU-R BS.1770 certified measurement.
[[nodiscard]] MasteringMetrics analyzeForMastering(
    const StereoAudioBuffer& audio,
    std::size_t maximumInspectedFrames = 1'500'000);

[[nodiscard]] double recommendedLoudnessTrimDb(
    const MasteringMetrics& metrics,
    double targetLufs,
    double maximumAbsoluteTrimDb = 6.0);

// Matches the v0.28.1 A/B rule: attenuate only the louder side so comparison
// never gains level merely to reach the other side.
[[nodiscard]] double matchedPreviewAttenuationDb(
    const MasteringMetrics& selected,
    const MasteringMetrics& other);
}
