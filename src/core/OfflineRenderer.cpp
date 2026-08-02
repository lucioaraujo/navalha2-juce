#include "core/OfflineRenderer.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace navalha
{
OfflineRender renderOffline(AudioEngine& engine,
                            std::size_t sampleCount,
                            std::size_t blockSize)
{
    if (sampleCount > maxOfflineSamples)
        throw std::length_error("Offline render exceeds the 10 million sample safety limit");
    if (blockSize == 0)
        throw std::invalid_argument("Offline block size must be positive");

    OfflineRender render;
    render.left.resize(sampleCount);
    render.right.resize(sampleCount);

    std::size_t offset = 0;
    while (offset < sampleCount)
    {
        const auto count = std::min(blockSize, sampleCount - offset);
        engine.processBlock(render.left.data() + offset, render.right.data() + offset, count);
        offset += count;
    }

    constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
    constexpr std::uint64_t fnvPrime = 1099511628211ULL;
    render.checksum = fnvOffset;
    long double squareSum = 0.0L;

    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        const auto left = render.left[sample];
        const auto right = render.right[sample];
        render.peak = std::max({render.peak, std::abs(left), std::abs(right)});
        squareSum += static_cast<long double>(left) * left
            + static_cast<long double>(right) * right;

        render.checksum ^= std::bit_cast<std::uint32_t>(left);
        render.checksum *= fnvPrime;
        render.checksum ^= std::bit_cast<std::uint32_t>(right);
        render.checksum *= fnvPrime;
    }

    if (sampleCount > 0)
        render.meanSquare = static_cast<double>(
            squareSum / static_cast<long double>(sampleCount * 2));
    return render;
}
}
