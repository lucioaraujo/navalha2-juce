#include "core/MasteringAnalysis.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace navalha
{
namespace
{
double amplitudeDb(double amplitude) noexcept
{
    return 20.0 * std::log10(std::max(1.0e-12, amplitude));
}
}

MasteringMetrics analyzeForMastering(const StereoAudioBuffer& audio,
                                     std::size_t maximumInspectedFrames)
{
    if (maximumInspectedFrames == 0)
        throw std::invalid_argument("Mastering analysis limit must be positive");

    MasteringMetrics metrics;
    const auto step = std::max<std::size_t>(
        1, audio.size() / maximumInspectedFrames);
    long double meanSquareSum = 0.0;
    long double crossSum = 0.0;
    long double leftSquareSum = 0.0;
    long double rightSquareSum = 0.0;

    for (std::size_t frame = 0; frame < audio.size(); frame += step)
    {
        const auto sample = audio.interpolated(static_cast<double>(frame));
        const auto left = static_cast<double>(sample.left);
        const auto right = static_cast<double>(sample.right);
        metrics.peak = std::max({metrics.peak, std::abs(left), std::abs(right)});
        meanSquareSum += (left * left + right * right) * 0.5;
        crossSum += left * right;
        leftSquareSum += left * left;
        rightSquareSum += right * right;
        ++metrics.inspectedFrames;
    }

    const auto denominator = static_cast<long double>(
        std::max<std::size_t>(1, metrics.inspectedFrames));
    const auto meanSquare = static_cast<double>(meanSquareSum / denominator);
    metrics.rms = std::sqrt(meanSquare);
    metrics.peakDb = amplitudeDb(metrics.peak);
    metrics.rmsDb = amplitudeDb(metrics.rms);
    metrics.estimatedLufs =
        -0.691 + 10.0 * std::log10(std::max(1.0e-12, meanSquare));
    metrics.crestDb = metrics.peakDb - metrics.rmsDb;
    metrics.correlation = static_cast<double>(
        crossSum / std::sqrt(std::max(
            1.0e-12L, leftSquareSum * rightSquareSum)));
    metrics.headroomDb = -metrics.peakDb;
    return metrics;
}

double recommendedLoudnessTrimDb(const MasteringMetrics& metrics,
                                 double targetLufs,
                                 double maximumAbsoluteTrimDb)
{
    if (!std::isfinite(targetLufs) || !std::isfinite(maximumAbsoluteTrimDb)
        || maximumAbsoluteTrimDb < 0.0)
        throw std::invalid_argument("Invalid loudness matching parameters");
    return std::clamp(targetLufs - metrics.estimatedLufs,
                      -maximumAbsoluteTrimDb,
                      maximumAbsoluteTrimDb);
}
}
