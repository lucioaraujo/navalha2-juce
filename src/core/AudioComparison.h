#pragma once

#include <cstddef>

#include "core/SlicePlayer.h"

namespace navalha
{
struct AudioComparison
{
    std::size_t frames = 0;
    double referenceRms = 0.0;
    double differenceRms = 0.0;
    double maximumAbsoluteDifference = 0.0;
    double correlation = 1.0;
    double signalToNoiseDb = 0.0;
};

[[nodiscard]] AudioComparison compareAudio(const StereoAudioBuffer& reference,
                                           const StereoAudioBuffer& candidate);
// Positive offsets skip leading candidate frames; negative offsets skip
// leading reference frames. Only the common aligned region is compared.
[[nodiscard]] AudioComparison compareAudioAligned(
    const StereoAudioBuffer& reference,
    const StereoAudioBuffer& candidate,
    std::ptrdiff_t candidateOffset);
}
