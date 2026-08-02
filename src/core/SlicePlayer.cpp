#include "core/SlicePlayer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace navalha
{
StereoAudioBuffer::StereoAudioBuffer(double sampleRate,
                                     std::vector<float> leftChannel,
                                     std::vector<float> rightChannel)
    : rate(sampleRate),
      left(std::move(leftChannel)),
      right(std::move(rightChannel))
{
    if (!std::isfinite(rate) || rate <= 0.0)
        throw std::invalid_argument("Buffer sample rate must be finite and positive");
    if (left.empty() || left.size() != right.size())
        throw std::invalid_argument("Stereo buffer channels must be non-empty and equal");
}

double StereoAudioBuffer::sampleRate() const noexcept
{
    return rate;
}

std::size_t StereoAudioBuffer::size() const noexcept
{
    return left.size();
}

StereoSample StereoAudioBuffer::interpolated(double position) const noexcept
{
    position = std::clamp(position, 0.0, static_cast<double>(left.size() - 1));
    const auto first = static_cast<std::size_t>(position);
    const auto second = std::min(first + 1, left.size() - 1);
    const auto fraction = static_cast<float>(position - static_cast<double>(first));

    return {
        left[first] + (left[second] - left[first]) * fraction,
        right[first] + (right[second] - right[first]) * fraction
    };
}

void SlicePlayer::prepare(double newOutputSampleRate)
{
    if (!std::isfinite(newOutputSampleRate) || newOutputSampleRate <= 0.0)
        throw std::invalid_argument("Output sample rate must be finite and positive");
    outputSampleRate = newOutputSampleRate;
    stopFadeLength = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::llround(outputSampleRate * 0.005)));
}

void SlicePlayer::setBuffer(const StereoAudioBuffer* newBuffer) noexcept
{
    playing = false;
    buffer = newBuffer;
}

void SlicePlayer::trigger(Slice slice,
                          bool reverse,
                          double playbackRate,
                          double attackSeconds,
                          double releaseSeconds)
{
    if (buffer == nullptr)
        throw std::logic_error("A buffer must be assigned before triggering a slice");
    if (!slice.isValid())
        throw std::invalid_argument("Slice boundaries are invalid");

    if (!std::isfinite(playbackRate) || playbackRate <= 0.0)
        throw std::invalid_argument("Playback rate must be finite and positive");

    static_cast<void>(
        tryTrigger(slice, reverse, playbackRate, attackSeconds, releaseSeconds));
}

bool SlicePlayer::tryTrigger(Slice slice,
                             bool reverse,
                             double playbackRate,
                             double attackSeconds,
                             double releaseSeconds) noexcept
{
    if (buffer == nullptr || !slice.isValid()
        || !std::isfinite(playbackRate) || playbackRate <= 0.0)
        return false;

    const auto sourceLength = static_cast<double>(buffer->size());
    const auto start = slice.start * sourceLength;
    const auto end = slice.end * sourceLength;
    increment = buffer->sampleRate() / outputSampleRate * playbackRate;
    totalSamples = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::ceil((end - start) / increment)));

    const auto durationSeconds = static_cast<double>(totalSamples) / outputSampleRate;
    const auto adaptiveFade = std::clamp(durationSeconds * 0.24, 0.0005, 0.005);
    const auto attack = attackSeconds >= 0.0
        ? std::min(attackSeconds, durationSeconds * 0.45)
        : adaptiveFade;
    const auto release = releaseSeconds >= 0.0
        ? std::min(releaseSeconds, durationSeconds * 0.48)
        : adaptiveFade;
    attackSamples = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::llround(attack * outputSampleRate)));
    releaseSamples = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::llround(release * outputSampleRate)));

    reversePlayback = reverse;
    position = reverse ? std::max(start, end - increment) : start;
    renderedSamples = 0;
    stopFadeRemaining = 0;
    stopping = false;
    playing = true;
    return true;
}

void SlicePlayer::stop() noexcept
{
    if (playing && !stopping)
    {
        stopping = true;
        stopFadeRemaining = stopFadeLength;
    }
}

StereoSample SlicePlayer::process() noexcept
{
    if (!playing || buffer == nullptr)
        return {};

    auto output = buffer->interpolated(position);
    auto gain = envelope();

    if (stopFadeRemaining > 0)
    {
        gain *= static_cast<float>(stopFadeRemaining)
            / static_cast<float>(stopFadeLength);
        --stopFadeRemaining;
    }

    output.left *= gain;
    output.right *= gain;
    ++renderedSamples;
    position += reversePlayback ? -increment : increment;

    if (renderedSamples >= totalSamples || (stopping && stopFadeRemaining == 0))
        playing = false;

    return output;
}

bool SlicePlayer::isPlaying() const noexcept
{
    return playing;
}

float SlicePlayer::envelope() const noexcept
{
    if (totalSamples <= 1)
        return 0.0F;

    const auto attack = std::min(
        1.0F, static_cast<float>(renderedSamples) / static_cast<float>(attackSamples));
    const auto samplesAfterCurrent = totalSamples - 1 - renderedSamples;
    const auto release = std::min(
        1.0F, static_cast<float>(samplesAfterCurrent) / static_cast<float>(releaseSamples));
    return std::min(attack, release);
}
}
