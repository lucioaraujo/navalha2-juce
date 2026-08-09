#pragma once

#include <array>
#include <cstddef>

#include "core/StereoMixer.h"
#include "core/TruePeakDetector.h"

namespace navalha
{
struct LookaheadLimiterParameters
{
    float ceilingDbtp = -1.0F;
    float lookaheadMilliseconds = 5.0F;
    float releaseMilliseconds = 80.0F;
    // Conservative allowance for detector/filter and gain-envelope error.
    float safetyMarginDb = 0.2F;
};

struct LookaheadLimiterTelemetry
{
    float inputTruePeak = 0.0F;
    float outputTruePeak = 0.0F;
    float gainReductionDb = 0.0F;
    bool ceilingEngaged = false;
};

class LookaheadLimiter
{
public:
    void prepare(double sampleRate,
                 LookaheadLimiterParameters parameters = {});
    void reset() noexcept;
    [[nodiscard]] StereoSample process(StereoSample input) noexcept;
    [[nodiscard]] LookaheadLimiterTelemetry telemetry() const noexcept;
    [[nodiscard]] LookaheadLimiterTelemetry consumeTelemetry() noexcept;
    [[nodiscard]] std::size_t latencySamples() const noexcept;
    [[nodiscard]] float ceilingLinear() const noexcept;

private:
    static constexpr std::size_t maximumLookaheadSamples = 8192;

    std::array<float, maximumLookaheadSamples + 1> delayLeft {};
    std::array<float, maximumLookaheadSamples + 1> delayRight {};
    std::size_t delayLength = 2;
    std::size_t delayWrite = 0;
    std::size_t lookahead = 1;
    std::size_t attackLength = 1;
    std::size_t attackRemaining = 0;
    std::size_t holdRemaining = 0;
    float ceiling = 0.89125094F;
    float detectorCeiling = 0.87096359F;
    float currentGain = 1.0F;
    float targetGain = 1.0F;
    float attackIncrement = 0.0F;
    float releaseCoefficient = 0.999F;
    TruePeakDetector inputLeft;
    TruePeakDetector inputRight;
    TruePeakDetector outputLeft;
    TruePeakDetector outputRight;
    LookaheadLimiterTelemetry currentTelemetry;
};
}
