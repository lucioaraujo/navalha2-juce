#pragma once

#include <cstddef>

namespace navalha
{
struct StereoSample
{
    float left = 0.0F;
    float right = 0.0F;
};

class LinearRamp
{
public:
    void prepare(double sampleRate, double rampSeconds);
    void reset(float value) noexcept;
    void setTarget(float value) noexcept;
    [[nodiscard]] float next() noexcept;
    [[nodiscard]] float current() const noexcept;

private:
    float value = 1.0F;
    float target = 1.0F;
    float increment = 0.0F;
    std::size_t rampLength = 1;
    std::size_t remaining = 0;
};

class StereoChannelProcessor
{
public:
    void prepare(double sampleRate, double rampSeconds = 0.015);
    void reset(float level = 1.0F, float pan = 0.0F, float width = 1.0F) noexcept;
    void setParameters(float level, float pan, float width) noexcept;
    [[nodiscard]] StereoSample process(StereoSample input) noexcept;

private:
    LinearRamp levelRamp;
    LinearRamp leftPanRamp;
    LinearRamp rightPanRamp;
    LinearRamp widthRamp;
};

class StereoSourceMixer
{
public:
    void prepare(double sampleRate, double rampSeconds = 0.015);
    void reset() noexcept;
    void setSourceParameters(std::size_t sourceIndex,
                             float effectiveLevel,
                             float pan,
                             float width);
    [[nodiscard]] StereoSample process(StereoSample sourceA, StereoSample sourceB) noexcept;

private:
    StereoChannelProcessor sourceAProcessor;
    StereoChannelProcessor sourceBProcessor;
};
}
