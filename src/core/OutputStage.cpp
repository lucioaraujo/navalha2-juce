#include "core/OutputStage.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace navalha
{
void OutputStage::prepare(double sampleRate, OutputStageParameters parameters)
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
        throw std::invalid_argument("Output sample rate must be finite and positive");
    if (!std::isfinite(parameters.ceilingDb)
        || parameters.ceilingDb > 0.0F || parameters.ceilingDb < -24.0F)
        throw std::invalid_argument("Output ceiling must be between -24 and 0 dBFS");
    if (!std::isfinite(parameters.dcBlockerHz)
        || parameters.dcBlockerHz <= 0.0F
        || parameters.dcBlockerHz >= static_cast<float>(sampleRate * 0.25))
        throw std::invalid_argument("DC blocker frequency is outside the safe range");
    if (!std::isfinite(parameters.outputTrimDb)
        || parameters.outputTrimDb < -24.0F || parameters.outputTrimDb > 0.0F)
        throw std::invalid_argument("Output trim must be between -24 and 0 dB");
    if (!std::isfinite(parameters.trimRampMilliseconds)
        || parameters.trimRampMilliseconds <= 0.0F
        || parameters.trimRampMilliseconds > 1000.0F)
        throw std::invalid_argument("Output trim ramp must be between 0 and 1000 ms");
    if (!std::isfinite(parameters.muteRampMilliseconds)
        || parameters.muteRampMilliseconds <= 0.0F
        || parameters.muteRampMilliseconds > 1000.0F)
        throw std::invalid_argument("Output mute ramp must be between 0 and 1000 ms");
    if (!std::isfinite(parameters.releaseMilliseconds)
        || parameters.releaseMilliseconds <= 0.0F
        || parameters.releaseMilliseconds > 5000.0F)
        throw std::invalid_argument("Output release must be between 0 and 5000 ms");

    constexpr double pi = 3.14159265358979323846;
    ceiling = std::pow(10.0F, parameters.ceilingDb / 20.0F);
    dcCoefficient = static_cast<float>(std::exp(
        -2.0 * pi * static_cast<double>(parameters.dcBlockerHz) / sampleRate));
    trimDb = parameters.outputTrimDb;
    trimRamp.prepare(
        sampleRate, static_cast<double>(parameters.trimRampMilliseconds) * 0.001);
    muteRamp.prepare(
        sampleRate, static_cast<double>(parameters.muteRampMilliseconds) * 0.001);
    limiter.prepare(sampleRate, {
        parameters.ceilingDb,
        parameters.lookaheadMilliseconds,
        parameters.releaseMilliseconds,
        parameters.truePeakSafetyMarginDb
    });
    reset();
}

void OutputStage::reset() noexcept
{
    previousInputLeft = 0.0F;
    previousInputRight = 0.0F;
    previousOutputLeft = 0.0F;
    previousOutputRight = 0.0F;
    trimRamp.reset(std::pow(10.0F, trimDb / 20.0F));
    // Every prepare/reset begins silent and reaches the requested state through
    // the same click-free ramp used by an explicit unmute.
    muteRamp.reset(0.0F);
    muteRamp.setTarget(muted ? 0.0F : 1.0F);
    limiter.reset();
    currentTelemetry = {};
}

void OutputStage::setOutputTrimDb(float newTrimDb) noexcept
{
    if (!std::isfinite(newTrimDb)) return;
    trimDb = std::clamp(newTrimDb, -24.0F, 0.0F);
    trimRamp.setTarget(std::pow(10.0F, trimDb / 20.0F));
}

void OutputStage::setMuted(bool shouldMute) noexcept
{
    muted = shouldMute;
    muteRamp.setTarget(muted ? 0.0F : 1.0F);
}

StereoSample OutputStage::process(StereoSample input) noexcept
{
    input.left = finiteOrZero(input.left, currentTelemetry.nonFiniteSamples);
    input.right = finiteOrZero(input.right, currentTelemetry.nonFiniteSamples);
    input.left = removeDc(input.left, previousInputLeft, previousOutputLeft);
    input.right = removeDc(input.right, previousInputRight, previousOutputRight);
    const auto technicalGain = trimRamp.next() * muteRamp.next();
    input.left *= technicalGain;
    input.right *= technicalGain;

    const auto inputPeak = std::max(std::abs(input.left), std::abs(input.right));
    currentTelemetry.inputSamplePeak = std::max(
        currentTelemetry.inputSamplePeak, inputPeak);
    const auto output = limiter.process(input);
    const auto limiterInterval = limiter.consumeTelemetry();
    currentTelemetry.inputTruePeak = std::max(
        currentTelemetry.inputTruePeak, limiterInterval.inputTruePeak);
    currentTelemetry.outputTruePeak = std::max(
        currentTelemetry.outputTruePeak, limiterInterval.outputTruePeak);
    currentTelemetry.gainReductionDb = std::max(
        currentTelemetry.gainReductionDb, limiterInterval.gainReductionDb);
    currentTelemetry.sampleCeilingLatched =
        currentTelemetry.sampleCeilingLatched || limiterInterval.ceilingEngaged;

    const auto outputPeak = std::max(
        std::abs(output.left), std::abs(output.right));
    currentTelemetry.outputSamplePeak = std::max(
        currentTelemetry.outputSamplePeak, outputPeak);
    return output;
}

OutputStageTelemetry OutputStage::telemetry() const noexcept
{
    return currentTelemetry;
}

OutputStageTelemetry OutputStage::consumeTelemetry() noexcept
{
    const auto result = currentTelemetry;
    currentTelemetry = {};
    return result;
}

std::size_t OutputStage::latencySamples() const noexcept
{
    return limiter.latencySamples();
}

float OutputStage::ceilingLinear() const noexcept
{
    return ceiling;
}

float OutputStage::outputTrimDb() const noexcept
{
    return trimDb;
}

bool OutputStage::isMuted() const noexcept
{
    return muted;
}

float OutputStage::finiteOrZero(float sample, std::uint64_t& counter) noexcept
{
    if (std::isfinite(sample)) return sample;
    ++counter;
    return 0.0F;
}

float OutputStage::removeDc(float input,
                            float& previousInput,
                            float& previousOutput) noexcept
{
    const auto output = input - previousInput + dcCoefficient * previousOutput;
    previousInput = input;
    previousOutput = std::isfinite(output) ? output : 0.0F;
    return previousOutput;
}
}
