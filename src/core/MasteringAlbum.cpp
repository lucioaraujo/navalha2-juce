#include "core/MasteringAlbum.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace navalha
{
namespace
{
std::uint64_t secondsToFrames(double seconds, double sampleRate)
{
    const auto frames = seconds * sampleRate;
    if (frames > static_cast<double>(std::numeric_limits<std::uint64_t>::max()))
        throw std::length_error("Album timeline exceeds the frame counter");
    return static_cast<std::uint64_t>(std::llround(frames));
}
}

AlbumLayout planAlbumLayout(std::span<const AlbumTrackSettings> tracks,
                            double sampleRate)
{
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0
        || sampleRate > 384000.0)
        throw std::invalid_argument("Invalid album sample rate");
    if (tracks.size() > maximumAlbumTracks)
        throw std::length_error("Album exceeds the 99-track limit");

    AlbumLayout result;
    result.tracks.reserve(tracks.size());
    for (const auto& track : tracks)
    {
        if (!std::isfinite(track.durationSeconds)
            || !std::isfinite(track.trimDb)
            || !std::isfinite(track.gapAfterSeconds)
            || !std::isfinite(track.fadeInSeconds)
            || !std::isfinite(track.fadeOutSeconds)
            || track.durationSeconds < 0.0
            || track.trimDb < -12.0 || track.trimDb > 12.0
            || track.gapAfterSeconds < 0.0 || track.gapAfterSeconds > 30.0
            || track.fadeInSeconds < 0.0 || track.fadeInSeconds > 20.0
            || track.fadeOutSeconds < 0.0 || track.fadeOutSeconds > 20.0)
            throw std::invalid_argument("Invalid album track settings");

        AlbumTrackLayout layout;
        layout.startFrame = result.totalFrames;
        layout.audioFrames = secondsToFrames(track.durationSeconds, sampleRate);
        layout.gapFrames = secondsToFrames(track.gapAfterSeconds, sampleRate);
        const auto maximumFadeFrames = layout.audioFrames / 2;
        layout.fadeInFrames = std::min(
            secondsToFrames(track.fadeInSeconds, sampleRate), maximumFadeFrames);
        layout.fadeOutFrames = std::min(
            secondsToFrames(track.fadeOutSeconds, sampleRate), maximumFadeFrames);
        layout.linearGain = std::pow(10.0, track.trimDb / 20.0);

        if (layout.audioFrames > std::numeric_limits<std::uint64_t>::max()
                - result.totalFrames
            || layout.gapFrames > std::numeric_limits<std::uint64_t>::max()
                - result.totalFrames - layout.audioFrames)
            throw std::length_error("Album timeline exceeds the frame counter");
        result.totalFrames += layout.audioFrames + layout.gapFrames;
        result.tracks.push_back(layout);
    }
    return result;
}

double albumTrackEnvelopeGain(const AlbumTrackLayout& track,
                              std::uint64_t relativeFrame) noexcept
{
    if (relativeFrame >= track.audioFrames)
        return 0.0;
    auto envelope = 1.0;
    if (track.fadeInFrames > 0 && relativeFrame < track.fadeInFrames)
        envelope = static_cast<double>(relativeFrame)
            / static_cast<double>(track.fadeInFrames);
    if (track.fadeOutFrames > 0
        && relativeFrame >= track.audioFrames - track.fadeOutFrames)
        envelope = std::min(
            envelope,
            static_cast<double>(track.audioFrames - relativeFrame)
                / static_cast<double>(track.fadeOutFrames));
    return envelope * track.linearGain;
}
}
