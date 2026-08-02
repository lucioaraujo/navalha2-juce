#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/AudioEngine.h"

namespace navalha
{
constexpr std::size_t maxOfflineSamples = 10'000'000;

struct OfflineRender
{
    std::vector<float> left;
    std::vector<float> right;
    float peak = 0.0F;
    double meanSquare = 0.0;
    std::uint64_t checksum = 0;
};

[[nodiscard]] OfflineRender renderOffline(AudioEngine& engine,
                                          std::size_t sampleCount,
                                          std::size_t blockSize = 512);
}
