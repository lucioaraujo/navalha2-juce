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

// peak/rms/crestDb/correlation mirror the lightweight v0.28.1 MASTER meter,
// unchanged. estimatedLufs no longer does (18 ago. 2026, see
// AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md 3.6): it is now a real ITU-R BS.1770-4
// K-weighted, gated integrated loudness (see MasteringAnalysis.cpp) instead
// of a plain global RMS with a -0.691 dB offset - the audit's own opening
// verdict already draws this line ("paridade com o Pure Data/WebAudio e
// excelência de saída são critérios diferentes"). Still an internal
// estimate, not an EBU R128/ITU-R BS.1770 certified measurement - no
// certified reference implementation cross-check has been run, only the
// algorithm (K-weighting + absolute/relative gating) matches the standard.
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
