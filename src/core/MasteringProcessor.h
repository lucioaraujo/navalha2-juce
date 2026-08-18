#pragma once

#include <cstddef>
#include <vector>

#include "core/LookaheadLimiter.h"
#include "core/SlicePlayer.h"

namespace navalha
{
struct MasteringParameters
{
    double trimDb = 0.0;
    double highPassHz = 25.0;
    double lowShelfDb = 0.0;
    double presenceDb = 0.0;
    double highShelfDb = 0.0;
    double compressorThresholdDb = -10.0;
    double compressorRatio = 2.0;
    double width = 1.0;
    double saturation = 0.0;
    double ceilingDb = -1.0;
};

struct MasteringRender
{
    std::vector<float> left;
    std::vector<float> right;
};

class MasteringProcessor
{
public:
    void prepare(double sampleRate, const MasteringParameters& parameters);
    void reset() noexcept;
    [[nodiscard]] StereoSample process(StereoSample input) noexcept;

    struct Biquad
    {
        void reset() noexcept;
        [[nodiscard]] double process(double input) noexcept;
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;
    };

    struct StereoBiquad
    {
        Biquad left;
        Biquad right;
        void reset() noexcept;
        [[nodiscard]] StereoSample process(StereoSample input) noexcept;
    };

private:
    [[nodiscard]] double linkedCompressorGain(
        StereoSample sample, double thresholdDb, double ratio,
        double attackCoefficient, double releaseCoefficient,
        double& smoothedGain) noexcept;

    StereoBiquad highPass;
    StereoBiquad lowShelf;
    StereoBiquad presence;
    StereoBiquad highShelf;
    double trimGain = 1.0;
    double widthDirect = 1.0;
    double widthCross = 0.0;
    double saturationDrive = 1.0;
    double saturationNormalization = 1.0;
    // saturation == 0 must be a true bypass (no tanh at all), not tanh at
    // drive=1 renormalized - see TAREFAS/AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md
    // 3.5. tanh(d*x)/tanh(d) -> x as d -> 0 mathematically, but tanh(0)=0
    // makes that limit a 0/0 division in floating point, so this is an
    // explicit branch instead of relying on the limit.
    bool saturationBypass = true;
    double compressorGain = 1.0;
    double compressorThreshold = -10.0;
    double compressorRatio = 2.0;
    double compressorAttack = 0.0;
    double compressorRelease = 0.0;
    // Replaces the old ratio-20/no-lookahead "limiter" (linkedCompressorGain
    // reused with a steep ratio) - that stage could not guarantee the ceiling
    // on fast transients (measured overshooting +1.33 dBFS at a -1 dBFS
    // ceiling - see AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md 3.4). Reuses the same
    // true-peak lookahead limiter already trusted for the live OutputStage
    // instead of a second, weaker implementation.
    LookaheadLimiter limiter;
};

[[nodiscard]] MasteringRender renderMastering(
    const StereoAudioBuffer& audio,
    const MasteringParameters& parameters,
    std::size_t maximumFrames = 10'000'000);
}
