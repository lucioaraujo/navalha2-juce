#pragma once

#include <cstddef>
#include <vector>

#include "core/StereoMixer.h"

namespace navalha
{
class LegacyPitchChannel
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void setSemitones(int semitones) noexcept;
    [[nodiscard]] float process(float input) noexcept;

private:
    [[nodiscard]] float readDelay(double delaySamples) const noexcept;

    std::vector<float> delay;
    std::size_t writeIndex = 0;
    double rate = 44100.0;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    double highPassPreviousInput = 0.0;
    double highPassPreviousOutput = 0.0;
    double highPassCoefficient = 0.999;
};

class HeritagePitch
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void setSemitones(int semitones) noexcept;
    void setMode(float processedAmount) noexcept;
    [[nodiscard]] StereoSample process(StereoSample input) noexcept;

private:
    LegacyPitchChannel left;
    LegacyPitchChannel right;
    LinearRamp mode;
};
}
