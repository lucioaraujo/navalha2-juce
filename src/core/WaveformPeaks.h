#pragma once

#include <cstddef>
#include <vector>

#include "core/SlicePlayer.h"

namespace navalha
{
constexpr std::size_t maxWaveformBins = 8192;

struct WaveformPeak
{
    float minimumLeft = 0.0F;
    float maximumLeft = 0.0F;
    float minimumRight = 0.0F;
    float maximumRight = 0.0F;
};

[[nodiscard]] std::vector<WaveformPeak> buildWaveformPeaks(
    const StereoAudioBuffer& buffer,
    std::size_t requestedBins);
}
