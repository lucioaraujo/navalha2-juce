#include "core/AudioComparison.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace navalha
{
namespace
{
AudioComparison compareRegion(const StereoAudioBuffer& reference,
                              const StereoAudioBuffer& candidate,
                              std::size_t referenceStart,
                              std::size_t candidateStart,
                              std::size_t frameCount)
{
    if (std::abs(reference.sampleRate() - candidate.sampleRate()) > 1.0e-6)
        throw std::invalid_argument("Audio comparison requires equal sample rates");

    AudioComparison result;
    result.frames = frameCount;
    if (result.frames == 0)
        return result;

    long double referenceSquareSum = 0.0L;
    long double differenceSquareSum = 0.0L;
    long double candidateSquareSum = 0.0L;
    long double dotProduct = 0.0L;

    const auto accumulate = [&] (double expected, double actual)
    {
        const auto difference = actual - expected;
        referenceSquareSum += expected * expected;
        candidateSquareSum += actual * actual;
        differenceSquareSum += difference * difference;
        dotProduct += expected * actual;
        result.maximumAbsoluteDifference =
            std::max(result.maximumAbsoluteDifference, std::abs(difference));
    };

    for (std::size_t frame = 0; frame < result.frames; ++frame)
    {
        const auto expected = reference.interpolated(
            static_cast<double>(referenceStart + frame));
        const auto actual = candidate.interpolated(
            static_cast<double>(candidateStart + frame));
        accumulate(expected.left, actual.left);
        accumulate(expected.right, actual.right);
    }

    const auto sampleCount = static_cast<long double>(result.frames) * 2.0L;
    result.referenceRms =
        std::sqrt(static_cast<double>(referenceSquareSum / sampleCount));
    result.differenceRms =
        std::sqrt(static_cast<double>(differenceSquareSum / sampleCount));

    if (referenceSquareSum > 0.0L && candidateSquareSum > 0.0L)
        result.correlation = static_cast<double>(
            dotProduct / std::sqrt(referenceSquareSum * candidateSquareSum));
    else
        result.correlation =
            referenceSquareSum == candidateSquareSum ? 1.0 : 0.0;

    result.signalToNoiseDb = differenceSquareSum == 0.0L
        ? std::numeric_limits<double>::infinity()
        : 10.0 * std::log10(
            static_cast<double>(referenceSquareSum / differenceSquareSum));
    return result;
}
}

AudioComparison compareAudio(const StereoAudioBuffer& reference,
                             const StereoAudioBuffer& candidate)
{
    if (reference.size() != candidate.size())
        throw std::invalid_argument("Audio comparison requires equal frame counts");
    return compareRegion(reference, candidate, 0, 0, reference.size());
}

AudioComparison compareAudioAligned(const StereoAudioBuffer& reference,
                                    const StereoAudioBuffer& candidate,
                                    std::ptrdiff_t candidateOffset)
{
    if (candidateOffset == std::numeric_limits<std::ptrdiff_t>::min())
        throw std::invalid_argument("Audio alignment offset is outside the safe range");
    const auto referenceStart = candidateOffset < 0
        ? static_cast<std::size_t>(-candidateOffset) : 0U;
    const auto candidateStart = candidateOffset > 0
        ? static_cast<std::size_t>(candidateOffset) : 0U;
    if (referenceStart >= reference.size() || candidateStart >= candidate.size())
        throw std::invalid_argument("Audio alignment offset leaves no common frames");

    const auto frames = std::min(
        reference.size() - referenceStart,
        candidate.size() - candidateStart);
    return compareRegion(
        reference, candidate, referenceStart, candidateStart, frames);
}
}
