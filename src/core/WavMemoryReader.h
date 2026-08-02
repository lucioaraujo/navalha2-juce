#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "core/SlicePlayer.h"

namespace navalha
{
constexpr std::size_t maxDecodedAudioFrames = 50'000'000;

[[nodiscard]] std::unique_ptr<StereoAudioBuffer> decodeWav(
    std::span<const std::uint8_t> bytes,
    std::size_t frameLimit = maxDecodedAudioFrames);
}
