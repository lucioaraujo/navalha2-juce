#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace navalha
{
constexpr std::size_t maximumAlbumTracks = 99;

struct AlbumTrackSettings
{
    double durationSeconds = 0.0;
    double trimDb = 0.0;
    double gapAfterSeconds = 2.0;
    double fadeInSeconds = 0.0;
    double fadeOutSeconds = 0.0;
};

struct AlbumTrackLayout
{
    std::uint64_t startFrame = 0;
    std::uint64_t audioFrames = 0;
    std::uint64_t gapFrames = 0;
    std::uint64_t fadeInFrames = 0;
    std::uint64_t fadeOutFrames = 0;
    double linearGain = 1.0;
};

struct AlbumLayout
{
    std::vector<AlbumTrackLayout> tracks;
    std::uint64_t totalFrames = 0;
};

[[nodiscard]] AlbumLayout planAlbumLayout(
    std::span<const AlbumTrackSettings> tracks,
    double sampleRate);
[[nodiscard]] double albumTrackEnvelopeGain(
    const AlbumTrackLayout& track,
    std::uint64_t relativeFrame) noexcept;
}
