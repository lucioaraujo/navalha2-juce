#include "core/MasteringAnalysis.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace navalha
{
namespace
{
double amplitudeDb(double amplitude) noexcept
{
    return 20.0 * std::log10(std::max(1.0e-12, amplitude));
}

// ITU-R BS.1770-4 K-weighting, two cascaded biquads derived from the
// standard's analog prototypes (pre-filter/head model, then the RLB
// high-pass) via the bilinear transform at the buffer's own sample rate -
// not hardcoded 48 kHz coefficients, so this stays correct across the
// 8 kHz-384 kHz range MasteringProcessor already accepts. Same shape
// (b0/b1/b2/a1/a2, direct form I) as MasteringProcessor::Biquad, kept as
// its own small copy here rather than a cross-module dependency - see
// AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md 3.6.
struct KBiquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    [[nodiscard]] double process(double input) noexcept
    {
        const auto output = input * b0 + z1;
        z1 = input * b1 - output * a1 + z2;
        z2 = input * b2 - output * a2;
        return output;
    }
};

struct KWeightingFilter
{
    KBiquad preFilter;
    KBiquad rlbHighPass;

    void prepare(double sampleRate) noexcept
    {
        // Stage 1: shelving "pre-filter" modelling head diffraction,
        // +4 dB above ~1.7 kHz. Analog prototype constants from BS.1770-4
        // Annex 1 (f0, G, Q), bilinear-transformed here at the real
        // sample rate instead of reusing fixed 48 kHz digital coefficients.
        {
            constexpr double f0 = 1681.9744509555319;
            constexpr double gainDb = 3.99984385397;
            constexpr double q = 0.7071752369554193;
            const auto k = std::tan(std::numbers::pi * f0 / sampleRate);
            const auto vh = std::pow(10.0, gainDb / 20.0);
            const auto vb = std::pow(vh, 0.4996667741545416);
            const auto a0 = 1.0 + k / q + k * k;
            preFilter.b0 = (vh + vb * k / q + k * k) / a0;
            preFilter.b1 = 2.0 * (k * k - vh) / a0;
            preFilter.b2 = (vh - vb * k / q + k * k) / a0;
            preFilter.a1 = 2.0 * (k * k - 1.0) / a0;
            preFilter.a2 = (1.0 - k / q + k * k) / a0;
        }
        // Stage 2: RLB weighting, a high-pass around 38 Hz that models
        // reduced sensitivity to very low frequencies.
        {
            constexpr double f0 = 38.13547087602444;
            constexpr double q = 0.5003270373238773;
            const auto k = std::tan(std::numbers::pi * f0 / sampleRate);
            const auto a0 = 1.0 + k / q + k * k;
            rlbHighPass.b0 = 1.0 / a0;
            rlbHighPass.b1 = -2.0 / a0;
            rlbHighPass.b2 = 1.0 / a0;
            rlbHighPass.a1 = 2.0 * (k * k - 1.0) / a0;
            rlbHighPass.a2 = (1.0 - k / q + k * k) / a0;
        }
        preFilter.z1 = preFilter.z2 = 0.0;
        rlbHighPass.z1 = rlbHighPass.z2 = 0.0;
    }

    [[nodiscard]] double process(double input) noexcept
    {
        return rlbHighPass.process(preFilter.process(input));
    }
};

// Gated integrated loudness per BS.1770-4: 400 ms blocks, 75% overlap
// (100 ms hop), absolute gate at -70 LUFS, then a relative gate 10 LU
// below the mean of the blocks that already passed the absolute gate.
// Analyzed contiguously (no stride) over up to maximumInspectedFrames -
// the previous implementation's decimated stride sampling made this both
// impossible (BS.1770 blocks must be contiguous audio, not a sparse
// subsample) and, incidentally, was the cause of the audit's secondary
// complaint about long files "losing the real peak" (see
// AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md 3.6) - peak is measured over the
// same contiguous range now, so nothing inside that range is skipped.
double gatedIntegratedLufs(const StereoAudioBuffer& audio,
                           std::size_t frameCount,
                           double sampleRate) noexcept
{
    const auto blockFrames = static_cast<std::size_t>(std::max(1.0, 0.400 * sampleRate));
    const auto hopFrames = static_cast<std::size_t>(std::max(1.0, 0.100 * sampleRate));
    if (frameCount < blockFrames)
        return -120.691;

    KWeightingFilter left;
    KWeightingFilter right;
    left.prepare(sampleRate);
    right.prepare(sampleRate);

    std::vector<double> blockMeanSquares;

    // Ring of partially-filled blocks (75% overlap means up to 4 blocks
    // are accumulating at once) - simplest correct approach: walk once,
    // and for every hop boundary start a fresh accumulator that runs for
    // blockFrames samples.
    struct ActiveBlock { double sum = 0.0; std::size_t remaining; };
    std::vector<ActiveBlock> active;

    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        const auto sample = audio.interpolated(static_cast<double>(frame));
        const auto weightedLeft = left.process(static_cast<double>(sample.left));
        const auto weightedRight = right.process(static_cast<double>(sample.right));
        const auto instantaneous = weightedLeft * weightedLeft + weightedRight * weightedRight;

        if (frame % hopFrames == 0)
            active.push_back({0.0, blockFrames});

        for (auto& block : active)
        {
            block.sum += instantaneous;
            --block.remaining;
        }
        for (std::size_t i = 0; i < active.size();)
        {
            if (active[i].remaining == 0)
            {
                blockMeanSquares.push_back(active[i].sum / static_cast<double>(blockFrames));
                active.erase(active.begin() + static_cast<std::ptrdiff_t>(i));
            }
            else ++i;
        }
    }

    if (blockMeanSquares.empty())
        return -120.691;

    const auto blockLoudness = [] (double meanSquare) noexcept
    { return -0.691 + 10.0 * std::log10(std::max(1.0e-12, meanSquare)); };

    // Absolute gate.
    double absoluteSum = 0.0;
    std::size_t absoluteCount = 0;
    for (const auto meanSquare : blockMeanSquares)
    {
        if (blockLoudness(meanSquare) >= -70.0)
        {
            absoluteSum += meanSquare;
            ++absoluteCount;
        }
    }
    if (absoluteCount == 0)
        return -120.691;
    const auto ungatedLoudness = blockLoudness(absoluteSum / static_cast<double>(absoluteCount));

    // Relative gate, 10 LU below the absolute-gated mean.
    double relativeSum = 0.0;
    std::size_t relativeCount = 0;
    for (const auto meanSquare : blockMeanSquares)
    {
        if (blockLoudness(meanSquare) >= -70.0
            && blockLoudness(meanSquare) >= ungatedLoudness - 10.0)
        {
            relativeSum += meanSquare;
            ++relativeCount;
        }
    }
    if (relativeCount == 0)
        return ungatedLoudness;
    return blockLoudness(relativeSum / static_cast<double>(relativeCount));
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
    // Real BS.1770-4 K-weighted gated loudness (see gatedIntegratedLufs
    // above), analyzed contiguously over up to maximumInspectedFrames -
    // no longer the plain global RMS with a -0.691 offset and no
    // weighting/gating that AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md 3.6
    // flagged. Still named/kept as an estimate (not a certified
    // EBU R128 measurement - no true-peak-limited pre-processing chain
    // requirement, no channel-count-specific weighting beyond stereo),
    // per this header's own existing disclaimer.
    metrics.estimatedLufs = gatedIntegratedLufs(
        audio,
        std::min(audio.size(), maximumInspectedFrames),
        audio.sampleRate());
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

double matchedPreviewAttenuationDb(
    const MasteringMetrics& selected,
    const MasteringMetrics& other)
{
    if (!std::isfinite(selected.estimatedLufs)
        || !std::isfinite(other.estimatedLufs))
        throw std::invalid_argument("Invalid A/B loudness estimates");
    return std::min(
        0.0, other.estimatedLufs - selected.estimatedLufs);
}
}
