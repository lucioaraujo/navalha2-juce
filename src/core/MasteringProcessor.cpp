#include "core/MasteringProcessor.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace navalha
{
namespace
{
double dbGain(double decibels) noexcept
{
    return std::pow(10.0, decibels / 20.0);
}

double smoothingCoefficient(double seconds, double sampleRate) noexcept
{
    return std::exp(-1.0 / (std::max(1.0e-6, seconds) * sampleRate));
}

void normalize(MasteringProcessor::Biquad& filter,
               double b0, double b1, double b2,
               double a0, double a1, double a2)
{
    filter.b0 = b0 / a0;
    filter.b1 = b1 / a0;
    filter.b2 = b2 / a0;
    filter.a1 = a1 / a0;
    filter.a2 = a2 / a0;
}

void configureHighPass(MasteringProcessor::Biquad& filter,
                       double sampleRate, double frequency)
{
    const auto omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    const auto cosine = std::cos(omega);
    const auto alpha = std::sin(omega) / (2.0 * std::numbers::sqrt2);
    normalize(filter, (1.0 + cosine) * 0.5, -(1.0 + cosine),
              (1.0 + cosine) * 0.5, 1.0 + alpha,
              -2.0 * cosine, 1.0 - alpha);
}

void configurePeaking(MasteringProcessor::Biquad& filter,
                      double sampleRate, double frequency,
                      double q, double gainDb)
{
    const auto amplitude = std::pow(10.0, gainDb / 40.0);
    const auto omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    const auto alpha = std::sin(omega) / (2.0 * q);
    const auto cosine = std::cos(omega);
    normalize(filter, 1.0 + alpha * amplitude, -2.0 * cosine,
              1.0 - alpha * amplitude, 1.0 + alpha / amplitude,
              -2.0 * cosine, 1.0 - alpha / amplitude);
}

void configureShelf(MasteringProcessor::Biquad& filter,
                    double sampleRate, double frequency,
                    double gainDb, bool high)
{
    const auto amplitude = std::pow(10.0, gainDb / 40.0);
    const auto omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    const auto cosine = std::cos(omega);
    const auto plus = amplitude + 1.0;
    const auto minus = amplitude - 1.0;
    const auto root = std::sin(omega) * std::sqrt(2.0 * amplitude);
    if (high)
        normalize(filter,
                  amplitude * (plus + minus * cosine + root),
                  -2.0 * amplitude * (minus + plus * cosine),
                  amplitude * (plus + minus * cosine - root),
                  plus - minus * cosine + root,
                  2.0 * (minus - plus * cosine),
                  plus - minus * cosine - root);
    else
        normalize(filter,
                  amplitude * (plus - minus * cosine + root),
                  2.0 * amplitude * (minus - plus * cosine),
                  amplitude * (plus - minus * cosine - root),
                  plus + minus * cosine + root,
                  -2.0 * (minus + plus * cosine),
                  plus + minus * cosine - root);
}

void copyCoefficients(MasteringProcessor::StereoBiquad& stereo,
                      const MasteringProcessor::Biquad& mono)
{
    stereo.left = mono;
    stereo.right = mono;
}
}

void MasteringProcessor::Biquad::reset() noexcept { z1 = z2 = 0.0; }

double MasteringProcessor::Biquad::process(double input) noexcept
{
    const auto output = input * b0 + z1;
    z1 = input * b1 - output * a1 + z2;
    z2 = input * b2 - output * a2;
    return output;
}

void MasteringProcessor::StereoBiquad::reset() noexcept
{
    left.reset();
    right.reset();
}

StereoSample MasteringProcessor::StereoBiquad::process(StereoSample input) noexcept
{
    return {static_cast<float>(left.process(input.left)),
            static_cast<float>(right.process(input.right))};
}

void MasteringProcessor::prepare(double sampleRate,
                                 const MasteringParameters& parameters)
{
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0
        || sampleRate > 384000.0
        || !std::isfinite(parameters.trimDb)
        || !std::isfinite(parameters.highPassHz)
        || !std::isfinite(parameters.lowShelfDb)
        || !std::isfinite(parameters.presenceDb)
        || !std::isfinite(parameters.highShelfDb)
        || !std::isfinite(parameters.compressorThresholdDb)
        || !std::isfinite(parameters.compressorRatio)
        || !std::isfinite(parameters.width)
        || !std::isfinite(parameters.saturation)
        || !std::isfinite(parameters.ceilingDb)
        || parameters.trimDb < -24.0 || parameters.trimDb > 24.0
        || parameters.highPassHz < 5.0
        || parameters.highPassHz > std::min(500.0, sampleRate * 0.45)
        || parameters.lowShelfDb < -12.0 || parameters.lowShelfDb > 12.0
        || parameters.presenceDb < -12.0 || parameters.presenceDb > 12.0
        || parameters.highShelfDb < -12.0 || parameters.highShelfDb > 12.0
        || parameters.compressorThresholdDb < -60.0
        || parameters.compressorThresholdDb > 0.0
        || parameters.compressorRatio < 1.0 || parameters.compressorRatio > 20.0
        || parameters.width < 0.0 || parameters.width > 2.0
        || parameters.saturation < 0.0 || parameters.saturation > 1.0
        || parameters.ceilingDb < -12.0 || parameters.ceilingDb > 0.0)
        throw std::invalid_argument("Invalid TRACK MASTER parameters");

    Biquad configured;
    configureHighPass(configured, sampleRate, parameters.highPassHz);
    copyCoefficients(highPass, configured);
    configureShelf(configured, sampleRate, 120.0, parameters.lowShelfDb, false);
    copyCoefficients(lowShelf, configured);
    configurePeaking(configured, sampleRate, 3200.0, 0.7, parameters.presenceDb);
    copyCoefficients(presence, configured);
    configureShelf(configured, sampleRate, 9000.0, parameters.highShelfDb, true);
    copyCoefficients(highShelf, configured);

    trimGain = dbGain(parameters.trimDb);
    widthDirect = (1.0 + parameters.width) * 0.5;
    widthCross = (1.0 - parameters.width) * 0.5;
    saturationBypass = parameters.saturation <= 0.0;
    saturationDrive = 1.0 + parameters.saturation * 24.0;
    saturationNormalization = 1.0 / std::tanh(saturationDrive);
    compressorThreshold = parameters.compressorThresholdDb;
    compressorRatio = parameters.compressorRatio;
    compressorAttack = smoothingCoefficient(0.030, sampleRate);
    compressorRelease = smoothingCoefficient(0.220, sampleRate);
    limiter.prepare(sampleRate, LookaheadLimiterParameters {
        static_cast<float>(parameters.ceilingDb), 5.0F, 80.0F, 0.2F });
    reset();
}

void MasteringProcessor::reset() noexcept
{
    highPass.reset();
    lowShelf.reset();
    presence.reset();
    highShelf.reset();
    compressorGain = 1.0;
    limiter.reset();
}

double MasteringProcessor::linkedCompressorGain(
    StereoSample sample, double thresholdDb, double ratio,
    double attackCoefficient, double releaseCoefficient,
    double& smoothedGain) noexcept
{
    const auto level = std::max(std::abs(static_cast<double>(sample.left)),
                                std::abs(static_cast<double>(sample.right)));
    const auto levelDb = 20.0 * std::log10(std::max(1.0e-12, level));
    const auto reductionDb = levelDb > thresholdDb
        ? (levelDb - thresholdDb) * (1.0 - 1.0 / ratio) : 0.0;
    const auto target = dbGain(-reductionDb);
    const auto coefficient = target < smoothedGain
        ? attackCoefficient : releaseCoefficient;
    smoothedGain = target + coefficient * (smoothedGain - target);
    return smoothedGain;
}

StereoSample MasteringProcessor::process(StereoSample input) noexcept
{
    input.left = static_cast<float>(input.left * trimGain);
    input.right = static_cast<float>(input.right * trimGain);
    auto sample = highShelf.process(
        presence.process(lowShelf.process(highPass.process(input))));
    const auto compressor = linkedCompressorGain(
        sample, compressorThreshold, compressorRatio,
        compressorAttack, compressorRelease, compressorGain);
    sample.left = static_cast<float>(sample.left * compressor);
    sample.right = static_cast<float>(sample.right * compressor);
    if (!saturationBypass)
    {
        sample.left = static_cast<float>(
            std::tanh(saturationDrive * sample.left) * saturationNormalization);
        sample.right = static_cast<float>(
            std::tanh(saturationDrive * sample.right) * saturationNormalization);
    }

    const StereoSample widened {
        static_cast<float>(sample.left * widthDirect + sample.right * widthCross),
        static_cast<float>(sample.left * widthCross + sample.right * widthDirect)
    };
    return limiter.process(widened);
}

MasteringRender renderMastering(const StereoAudioBuffer& audio,
                                const MasteringParameters& parameters,
                                std::size_t maximumFrames)
{
    if (audio.size() > maximumFrames)
        throw std::length_error("TRACK MASTER render exceeds its frame limit");
    MasteringProcessor processor;
    processor.prepare(audio.sampleRate(), parameters);
    MasteringRender result;
    result.left.resize(audio.size());
    result.right.resize(audio.size());
    for (std::size_t frame = 0; frame < audio.size(); ++frame)
    {
        const auto output = processor.process(
            audio.interpolated(static_cast<double>(frame)));
        result.left[frame] = output.left;
        result.right[frame] = output.right;
    }
    return result;
}
}
