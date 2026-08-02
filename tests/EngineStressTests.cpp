#include "core/AudioEngine.h"
#include "core/SessionModel.h"
#include "core/SlicePlayer.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <string>
#include <stdexcept>
#include <vector>

namespace
{
struct StressResult
{
    std::uint64_t checksum = 14695981039346656037ULL;
    double peak = 0.0;
    long double squareSum = 0.0L;
};

void hashSample(StressResult& result, float sample)
{
    if (!std::isfinite(sample))
        throw std::runtime_error("Stress render produced a non-finite sample");
    result.checksum ^= std::bit_cast<std::uint32_t>(sample);
    result.checksum *= 1099511628211ULL;
    result.peak = std::max(result.peak, std::abs(static_cast<double>(sample)));
    result.squareSum += static_cast<long double>(sample) * sample;
}

StressResult renderStress(std::size_t blockSize, std::size_t renderFrames)
{
    constexpr double sampleRate = 48000.0;
    constexpr std::size_t sourceFrames = 48000;
    std::vector<float> leftA(sourceFrames);
    std::vector<float> rightA(sourceFrames);
    std::vector<float> leftB(sourceFrames);
    std::vector<float> rightB(sourceFrames);
    for (std::size_t frame = 0; frame < sourceFrames; ++frame)
    {
        const auto time = static_cast<double>(frame) / sampleRate;
        leftA[frame] = static_cast<float>(
            0.37 * std::sin(2.0 * std::numbers::pi * 173.0 * time));
        rightA[frame] = static_cast<float>(
            0.31 * std::sin(2.0 * std::numbers::pi * 263.0 * time));
        leftB[frame] = static_cast<float>(
            0.29 * std::sin(2.0 * std::numbers::pi * 347.0 * time));
        rightB[frame] = static_cast<float>(
            0.23 * std::sin(2.0 * std::numbers::pi * 431.0 * time));
    }

    navalha::StereoAudioBuffer sourceA(
        sampleRate, std::move(leftA), std::move(rightA));
    navalha::StereoAudioBuffer sourceB(
        sampleRate, std::move(leftB), std::move(rightB));
    navalha::SessionModel session;
    for (std::size_t step = 0; step < navalha::stepsPerPattern; ++step)
        session.patterns.setCell(
            0, step, static_cast<std::uint16_t>(
                step % 2 == 0 ? step : 128 + step));
    session.sequencer.setTempo(173.0, 3);
    session.sequencer.setTiming(navalha::TimingMode::jitter, 31.0, 0x13579bdfU);
    session.heritagePitchSemitones = 7;
    session.heritagePitchMode = 0.72;
    session.masterLevel = 0.76;
    session.mixer.sourceA.pan = -0.22;
    session.mixer.sourceB.pan = 0.19;
    session.mixer.sourceA.width = 1.35;
    session.mixer.sourceB.width = 0.78;
    session.formDirector.setEnabled(true);
    session.assisted.enabled = true;
    session.assisted.editSlices = true;
    session.assisted.autoMix = true;
    session.assisted.useGaps = true;
    session.assisted.variation = 74;
    session.assistedRng.setSeed(0x2468ace0U);
    static_cast<void>(session.controlTrace.append(0, 173, 0));
    static_cast<void>(session.controlTrace.append(800, 131, -5));
    static_cast<void>(session.controlTrace.append(1600, 199, 7));
    for (std::size_t voice = 0; voice < session.virtualVoices.size(); ++voice)
    {
        auto& state = session.virtualVoices[voice];
        state.enabled = true;
        state.sourceIndex = voice;
        state.division = voice == 0 ? 2 : 4;
        state.pitchSemitones = voice == 0 ? -5 : 9;
        state.level = 0.24;
        state.pan = voice == 0 ? -0.4 : 0.4;
    }

    navalha::AudioEngine engine(session);
    engine.prepare(sampleRate);
    engine.setSourceBuffer(0, &sourceA);
    engine.setSourceBuffer(1, &sourceB);
    if (!engine.submitCommand({navalha::EngineCommandType::start}))
        throw std::runtime_error("Unable to start stress transport");
    if (!engine.submitCommand(
            {navalha::EngineCommandType::startTraceLoop, 1}))
        throw std::runtime_error("Unable to start stress TRACE LOOP");

    std::vector<float> left(blockSize);
    std::vector<float> right(blockSize);
    StressResult result;
    std::size_t rendered = 0;
    while (rendered < renderFrames)
    {
        const auto count = std::min(blockSize, renderFrames - rendered);
        engine.processBlock(left.data(), right.data(), count);
        for (std::size_t frame = 0; frame < count; ++frame)
        {
            hashSample(result, left[frame]);
            hashSample(result, right[frame]);
        }
        rendered += count;
    }
    if (!engine.transportTelemetry().running || result.squareSum <= 0.0L)
        throw std::runtime_error("Stress transport did not remain active");
    return result;
}
}

int main(int argc, char** argv)
{
    // Includes the v0.28.1 default Assisted REPEAT cadence.
    constexpr std::uint64_t expectedChecksum = 680956987478527214ULL;
    constexpr std::size_t defaultFrames = 1440000;
    std::size_t renderFrames = defaultFrames;
    if (argc == 3 && std::string(argv[1]) == "--seconds")
    {
        const auto seconds = std::stoull(argv[2]);
        if (seconds < 1 || seconds > 3600)
            throw std::invalid_argument("Stress duration must be between 1 and 3600 seconds");
        renderFrames = static_cast<std::size_t>(seconds) * 48000;
    }
    else if (argc != 1)
    {
        throw std::invalid_argument("Usage: navalha_engine_stress_tests [--seconds 1..3600]");
    }

    const auto smallBlocks = renderStress(64, renderFrames);
    const auto irregularBlocks = renderStress(511, renderFrames);
    if (smallBlocks.checksum != irregularBlocks.checksum
        || smallBlocks.peak != irregularBlocks.peak
        || smallBlocks.squareSum != irregularBlocks.squareSum)
        throw std::runtime_error("Stress render changed with host block size");
    if (renderFrames == defaultFrames && smallBlocks.checksum != expectedChecksum)
        throw std::runtime_error("Stress DSP signature changed: "
                                 + std::to_string(smallBlocks.checksum));
    std::cout << "stress_checksum=" << smallBlocks.checksum
              << " peak=" << smallBlocks.peak
              << " seconds=" << renderFrames / 48000 << '\n';
}
