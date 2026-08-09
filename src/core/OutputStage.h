#pragma once

#include <cstddef>
#include <cstdint>

#include "core/LookaheadLimiter.h"
#include "core/StereoMixer.h"

namespace navalha
{
struct OutputStageParameters
{
    float ceilingDb = -1.0F;
    float dcBlockerHz = 5.0F;
    float outputTrimDb = 0.0F;
    float trimRampMilliseconds = 20.0F;
    float muteRampMilliseconds = 10.0F;
    float lookaheadMilliseconds = 5.0F;
    float releaseMilliseconds = 80.0F;
    float truePeakSafetyMarginDb = 0.2F;
};

struct OutputStageTelemetry
{
    float inputSamplePeak = 0.0F;
    float outputSamplePeak = 0.0F;
    float inputTruePeak = 0.0F;
    float outputTruePeak = 0.0F;
    float gainReductionDb = 0.0F;
    std::uint64_t nonFiniteSamples = 0;
    bool sampleCeilingLatched = false;
};

// Final live-safety boundary: finite guard, DC rejection and a stereo-linked
// lookahead limiter measured with a 4x true-peak detector. Its declared latency
// is the configured lookahead; legacy/offline parity bypasses this stage in the
// AudioEngine rather than weakening the live contract.
class OutputStage
{
public:
    void prepare(double sampleRate,
                 OutputStageParameters parameters = {});
    void reset() noexcept;
    void setOutputTrimDb(float trimDb) noexcept;
    void setMuted(bool shouldMute) noexcept;
    [[nodiscard]] StereoSample process(StereoSample input) noexcept;
    [[nodiscard]] OutputStageTelemetry telemetry() const noexcept;
    // Audio-thread interval read: clears counters/peaks but preserves DSP state.
    [[nodiscard]] OutputStageTelemetry consumeTelemetry() noexcept;
    [[nodiscard]] std::size_t latencySamples() const noexcept;
    [[nodiscard]] float ceilingLinear() const noexcept;
    [[nodiscard]] float outputTrimDb() const noexcept;
    [[nodiscard]] bool isMuted() const noexcept;

private:
    [[nodiscard]] static float finiteOrZero(float sample,
                                            std::uint64_t& counter) noexcept;
    [[nodiscard]] float removeDc(float input,
                                 float& previousInput,
                                 float& previousOutput) noexcept;

    float ceiling = 0.89125094F;
    float dcCoefficient = 0.999F;
    float trimDb = 0.0F;
    bool muted = false;
    float previousInputLeft = 0.0F;
    float previousInputRight = 0.0F;
    float previousOutputLeft = 0.0F;
    float previousOutputRight = 0.0F;
    LinearRamp trimRamp;
    LinearRamp muteRamp;
    LookaheadLimiter limiter;
    OutputStageTelemetry currentTelemetry;
};
}
