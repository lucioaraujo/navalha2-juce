#include "core/HeritagePitch.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace navalha
{
void LegacyPitchChannel::prepare(double sampleRate)
{
    rate = std::max(1.0, sampleRate);
    delay.assign(static_cast<std::size_t>(std::ceil(rate * 0.010)) + 4, 0.0F);
    highPassCoefficient = std::exp(-2.0 * std::numbers::pi * 5.0 / rate);
    reset();
}

void LegacyPitchChannel::reset() noexcept
{
    std::fill(delay.begin(), delay.end(), 0.0F);
    writeIndex = 0;
    phase = 0.0;
    highPassPreviousInput = 0.0;
    highPassPreviousOutput = 0.0;
}

void LegacyPitchChannel::setSemitones(int semitones) noexcept
{
    semitones = std::clamp(semitones, -12, 11);
    const auto ratio = std::exp(static_cast<double>(semitones) * 0.05776);
    phaseIncrement = (1.0 - ratio) / (0.010 * rate);
}

float LegacyPitchChannel::process(float input) noexcept
{
    if (delay.empty())
        return input;

    delay[writeIndex] = input;
    auto secondPhase = phase + 0.5;
    if (secondPhase >= 1.0)
        secondPhase -= 1.0;

    const auto windowA = std::sin(std::numbers::pi * phase);
    const auto windowB = std::sin(std::numbers::pi * secondPhase);
    const auto shifted = static_cast<double>(readDelay(phase * rate * 0.010)) * windowA
        + static_cast<double>(readDelay(secondPhase * rate * 0.010)) * windowB;

    writeIndex = (writeIndex + 1) % delay.size();
    phase += phaseIncrement;
    phase -= std::floor(phase);

    const auto filtered = shifted - highPassPreviousInput
        + highPassCoefficient * highPassPreviousOutput;
    highPassPreviousInput = shifted;
    highPassPreviousOutput = filtered;
    return static_cast<float>(filtered);
}

float LegacyPitchChannel::readDelay(double delaySamples) const noexcept
{
    const auto size = static_cast<double>(delay.size());
    auto position = static_cast<double>(writeIndex) - std::clamp(delaySamples, 0.0, size - 2.0);
    while (position < 0.0)
        position += size;

    const auto bIndex = static_cast<std::size_t>(position) % delay.size();
    const auto a = delay[(bIndex + delay.size() - 1) % delay.size()];
    const auto b = delay[bIndex];
    const auto c = delay[(bIndex + 1) % delay.size()];
    const auto d = delay[(bIndex + 2) % delay.size()];
    const auto fraction = static_cast<float>(position - std::floor(position));

    // Same four-point polynomial used by Pure Data's vd~ delay reader.
    const auto cMinusB = c - b;
    return b + fraction * (cMinusB
        - (1.0F / 6.0F) * (1.0F - fraction)
            * ((d - a - 3.0F * cMinusB) * fraction
                + d + 2.0F * a - 3.0F * b));
}

void HeritagePitch::prepare(double sampleRate)
{
    left.prepare(sampleRate);
    right.prepare(sampleRate);
    mode.prepare(sampleRate, 0.020);
    mode.reset(0.0F);
}

void HeritagePitch::reset() noexcept
{
    left.reset();
    right.reset();
}

void HeritagePitch::setSemitones(int semitones) noexcept
{
    left.setSemitones(semitones);
    right.setSemitones(semitones);
}

void HeritagePitch::setMode(float processedAmount) noexcept
{
    mode.setTarget(std::clamp(processedAmount, 0.0F, 1.0F));
}

StereoSample HeritagePitch::process(StereoSample input) noexcept
{
    const StereoSample shifted {left.process(input.left), right.process(input.right)};
    const auto wet = mode.next();
    const auto dry = 1.0F - wet;
    return {
        shifted.left * wet + input.left * dry,
        shifted.right * wet + input.right * dry
    };
}
}
