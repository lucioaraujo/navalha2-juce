#include "core/LookaheadLimiter.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace navalha
{
void LookaheadLimiter::prepare(double sampleRate,
                               LookaheadLimiterParameters parameters)
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
        throw std::invalid_argument("Limiter sample rate must be finite and positive");
    if (!std::isfinite(parameters.ceilingDbtp)
        || parameters.ceilingDbtp > 0.0F || parameters.ceilingDbtp < -24.0F)
        throw std::invalid_argument("Limiter ceiling must be between -24 and 0 dBTP");
    if (!std::isfinite(parameters.lookaheadMilliseconds)
        || parameters.lookaheadMilliseconds < 1.0F
        || parameters.lookaheadMilliseconds > 20.0F)
        throw std::invalid_argument("Limiter lookahead must be between 1 and 20 ms");
    if (!std::isfinite(parameters.releaseMilliseconds)
        || parameters.releaseMilliseconds <= 0.0F
        || parameters.releaseMilliseconds > 5000.0F)
        throw std::invalid_argument("Limiter release must be between 0 and 5000 ms");
    if (!std::isfinite(parameters.safetyMarginDb)
        || parameters.safetyMarginDb < 0.0F || parameters.safetyMarginDb > 3.0F)
        throw std::invalid_argument("Limiter safety margin must be between 0 and 3 dB");

    lookahead = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::llround(
            sampleRate * parameters.lookaheadMilliseconds * 0.001)));
    if (lookahead > maximumLookaheadSamples)
        throw std::invalid_argument("Limiter lookahead exceeds fixed realtime capacity");
    delayLength = lookahead + 1;
    ceiling = std::pow(10.0F, parameters.ceilingDbtp / 20.0F);
    detectorCeiling = std::pow(
        10.0F, (parameters.ceilingDbtp - parameters.safetyMarginDb) / 20.0F);
    releaseCoefficient = static_cast<float>(std::exp(
        -1.0 / (sampleRate
                * static_cast<double>(parameters.releaseMilliseconds) * 0.001)));

    inputLeft.prepare(sampleRate);
    inputRight.prepare(sampleRate);
    outputLeft.prepare(sampleRate);
    outputRight.prepare(sampleRate);
    const auto detectorDelay = std::max(
        inputLeft.latencySamples(), inputRight.latencySamples());
    attackLength = std::max<std::size_t>(
        1, lookahead > detectorDelay + 2
            ? lookahead - detectorDelay - 2 : 1);
    reset();
}

void LookaheadLimiter::reset() noexcept
{
    delayLeft.fill(0.0F);
    delayRight.fill(0.0F);
    delayWrite = 0;
    attackRemaining = 0;
    holdRemaining = 0;
    currentGain = 1.0F;
    targetGain = 1.0F;
    attackIncrement = 0.0F;
    inputLeft.reset();
    inputRight.reset();
    outputLeft.reset();
    outputRight.reset();
    currentTelemetry = {};
}

StereoSample LookaheadLimiter::process(StereoSample input) noexcept
{
    if (!std::isfinite(input.left)) input.left = 0.0F;
    if (!std::isfinite(input.right)) input.right = 0.0F;

    const auto detectedPeak = std::max(
        inputLeft.processSample(input.left),
        inputRight.processSample(input.right));
    currentTelemetry.inputTruePeak = std::max(
        currentTelemetry.inputTruePeak, detectedPeak);
    const auto requiredGain = detectedPeak > detectorCeiling && detectedPeak > 0.0F
        ? detectorCeiling / detectedPeak : 1.0F;

    if (requiredGain < 1.0F)
    {
        currentTelemetry.ceilingEngaged = true;
        holdRemaining = lookahead;
        if (requiredGain < targetGain)
        {
            targetGain = requiredGain;
            if (attackRemaining == 0)
                attackRemaining = attackLength;
            attackIncrement = (targetGain - currentGain)
                / static_cast<float>(std::max<std::size_t>(1, attackRemaining));
        }
    }

    if (attackRemaining > 0)
    {
        currentGain += attackIncrement;
        --attackRemaining;
        if (attackRemaining == 0)
            currentGain = targetGain;
    }
    else if (holdRemaining > 0)
        --holdRemaining;
    else
    {
        currentGain = 1.0F - (1.0F - currentGain) * releaseCoefficient;
        targetGain = currentGain;
    }
    currentGain = std::clamp(currentGain, 0.0F, 1.0F);

    delayLeft[delayWrite] = input.left;
    delayRight[delayWrite] = input.right;
    delayWrite = (delayWrite + 1) % delayLength;
    StereoSample output {
        delayLeft[delayWrite] * currentGain,
        delayRight[delayWrite] * currentGain
    };
    // Last-resort sample guard. The true-peak detector below remains the
    // authority for reconstructed-waveform compliance.
    output.left = std::clamp(output.left, -ceiling, ceiling);
    output.right = std::clamp(output.right, -ceiling, ceiling);

    currentTelemetry.outputTruePeak = std::max(
        currentTelemetry.outputTruePeak,
        std::max(outputLeft.processSample(output.left),
                 outputRight.processSample(output.right)));
    const auto reduction = currentGain > 0.0F
        ? std::max(0.0F, -20.0F * std::log10(currentGain)) : 120.0F;
    currentTelemetry.gainReductionDb = std::max(
        currentTelemetry.gainReductionDb, reduction);
    return output;
}

LookaheadLimiterTelemetry LookaheadLimiter::telemetry() const noexcept
{
    return currentTelemetry;
}

LookaheadLimiterTelemetry LookaheadLimiter::consumeTelemetry() noexcept
{
    const auto result = currentTelemetry;
    currentTelemetry = {};
    return result;
}

std::size_t LookaheadLimiter::latencySamples() const noexcept
{
    return lookahead;
}

float LookaheadLimiter::ceilingLinear() const noexcept
{
    return ceiling;
}
}
