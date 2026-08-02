#include "core/StereoMixer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace navalha
{
void LinearRamp::prepare(double sampleRate, double rampSeconds)
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
        throw std::invalid_argument("Sample rate must be finite and positive");
    if (!std::isfinite(rampSeconds) || rampSeconds < 0.0)
        throw std::invalid_argument("Ramp duration must be finite and non-negative");

    rampLength = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::llround(sampleRate * rampSeconds)));
}

void LinearRamp::reset(float newValue) noexcept
{
    value = newValue;
    target = newValue;
    increment = 0.0F;
    remaining = 0;
}

void LinearRamp::setTarget(float newTarget) noexcept
{
    if (newTarget == target)
        return;

    target = newTarget;
    remaining = rampLength;
    increment = (target - value) / static_cast<float>(remaining);
}

float LinearRamp::next() noexcept
{
    if (remaining > 0)
    {
        value += increment;
        --remaining;
        if (remaining == 0)
            value = target;
    }
    return value;
}

float LinearRamp::current() const noexcept
{
    return value;
}

void StereoChannelProcessor::prepare(double sampleRate, double rampSeconds)
{
    levelRamp.prepare(sampleRate, rampSeconds);
    leftPanRamp.prepare(sampleRate, rampSeconds);
    rightPanRamp.prepare(sampleRate, rampSeconds);
    widthRamp.prepare(sampleRate, rampSeconds);
}

void StereoChannelProcessor::reset(float level, float pan, float width) noexcept
{
    level = std::clamp(level, 0.0F, 1.25F);
    pan = std::clamp(pan, -1.0F, 1.0F);
    width = std::clamp(width, 0.0F, 2.0F);

    levelRamp.reset(level);
    leftPanRamp.reset(pan <= 0.0F ? 1.0F : 1.0F - pan);
    rightPanRamp.reset(pan >= 0.0F ? 1.0F : 1.0F + pan);
    widthRamp.reset(width);
}

void StereoChannelProcessor::setParameters(float level, float pan, float width) noexcept
{
    level = std::clamp(level, 0.0F, 1.25F);
    pan = std::clamp(pan, -1.0F, 1.0F);
    width = std::clamp(width, 0.0F, 2.0F);

    levelRamp.setTarget(level);
    leftPanRamp.setTarget(pan <= 0.0F ? 1.0F : 1.0F - pan);
    rightPanRamp.setTarget(pan >= 0.0F ? 1.0F : 1.0F + pan);
    widthRamp.setTarget(width);
}

StereoSample StereoChannelProcessor::process(StereoSample input) noexcept
{
    const auto middle = (input.left + input.right) * 0.5F;
    const auto side = (input.left - input.right) * 0.5F * widthRamp.next();
    const auto level = levelRamp.next();

    return {
        (middle + side) * leftPanRamp.next() * level,
        (middle - side) * rightPanRamp.next() * level
    };
}

void StereoSourceMixer::prepare(double sampleRate, double rampSeconds)
{
    sourceAProcessor.prepare(sampleRate, rampSeconds);
    sourceBProcessor.prepare(sampleRate, rampSeconds);
    reset();
}

void StereoSourceMixer::reset() noexcept
{
    sourceAProcessor.reset();
    sourceBProcessor.reset();
}

void StereoSourceMixer::setSourceParameters(std::size_t sourceIndex,
                                            float effectiveLevel,
                                            float pan,
                                            float width)
{
    if (sourceIndex > 1)
        throw std::out_of_range("Source index must be A/0 or B/1");

    auto& processor = sourceIndex == 0 ? sourceAProcessor : sourceBProcessor;
    processor.setParameters(effectiveLevel, pan, width);
}

StereoSample StereoSourceMixer::process(StereoSample sourceA, StereoSample sourceB) noexcept
{
    const auto a = sourceAProcessor.process(sourceA);
    const auto b = sourceBProcessor.process(sourceB);
    return {a.left + b.left, a.right + b.right};
}
}
