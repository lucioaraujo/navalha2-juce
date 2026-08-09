#include "core/AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace navalha
{
namespace
{
void publishPeak(std::atomic<float>& destination, float value) noexcept
{
    auto previous = destination.load(std::memory_order_relaxed);
    while (previous < value
           && !destination.compare_exchange_weak(
               previous, value,
               std::memory_order_release,
               std::memory_order_relaxed))
    {
    }
}
}

AudioEngine::AudioEngine(SessionModel& sessionModel) noexcept
    : session(sessionModel)
{
}

void AudioEngine::prepare(double sampleRate)
{
    outputSampleRate = sampleRate;
    tracePlayer.prepare(sampleRate);
    session.sequencer.prepare(sampleRate);
    mixer.prepare(sampleRate);
    pitch.prepare(sampleRate);
    pitch.setSemitones(session.heritagePitchSemitones);
    pitch.setMode(static_cast<float>(session.heritagePitchMode));
    masterLevel.prepare(sampleRate, 0.015);
    session.masterLevel = std::clamp(session.masterLevel, 0.0, 1.0);
    masterLevel.reset(static_cast<float>(session.masterLevel));
    appliedOutputTrimDb = requestedOutputTrimDb.load(std::memory_order_acquire);
    appliedOutputMuted = requestedOutputMuted.load(std::memory_order_acquire)
        || requestedOutputSuspended.load(std::memory_order_acquire);
    OutputStageParameters outputParameters;
    outputParameters.outputTrimDb = appliedOutputTrimDb;
    outputStage.prepare(sampleRate, outputParameters);
    outputStage.setMuted(appliedOutputMuted);

    for (auto& sourcePlayers : players)
        for (auto& player : sourcePlayers)
            player.prepare(sampleRate);
    for (auto& voicePlayers : virtualPlayers)
        for (auto& player : voicePlayers)
            player.prepare(sampleRate);
    for (auto& voiceMixer : virtualMixers)
        voiceMixer.prepare(sampleRate);

    syncMixerParameters();
}

void AudioEngine::setOutputProfile(OutputProfile profile) noexcept
{
    outputProfile = profile;
}

bool AudioEngine::setOutputTrimDb(float trimDb) noexcept
{
    if (!std::isfinite(trimDb) || trimDb < -24.0F || trimDb > 0.0F)
        return false;
    requestedOutputTrimDb.store(trimDb, std::memory_order_release);
    return true;
}

void AudioEngine::setOutputMuted(bool shouldMute) noexcept
{
    requestedOutputMuted.store(shouldMute, std::memory_order_release);
}

void AudioEngine::suspendOutput() noexcept
{
    requestedOutputSuspended.store(true, std::memory_order_release);
}

void AudioEngine::resumeOutput() noexcept
{
    requestedOutputSuspended.store(false, std::memory_order_release);
}

void AudioEngine::setSourceBuffer(std::size_t sourceIndex, const StereoAudioBuffer* buffer)
{
    if (sourceIndex >= players.size())
        throw std::out_of_range("Source index must be A/0 or B/1");

    sourceBuffers[sourceIndex] = buffer;
    for (auto& player : players[sourceIndex])
        player.setBuffer(buffer);
    session.sources[sourceIndex].hasAudio = buffer != nullptr;
}

void AudioEngine::syncMixerParameters()
{
    session.mixer.normalize();
    const std::array<const MixerChannel*, 2> channels {
        &session.mixer.sourceA, &session.mixer.sourceB
    };

    for (std::size_t sourceIndex = 0; sourceIndex < channels.size(); ++sourceIndex)
    {
        const auto& channel = *channels[sourceIndex];
        mixer.setSourceParameters(sourceIndex,
                                  static_cast<float>(session.mixer.effectiveLevel(sourceIndex)),
                                  static_cast<float>(channel.pan),
                                  static_cast<float>(channel.width));
    }
}

void AudioEngine::start() noexcept
{
    session.sequencer.start();
}

void AudioEngine::stop() noexcept
{
    session.sequencer.stop();
    tracePlayer.stop();
    for (auto& sourcePlayers : players)
        for (auto& player : sourcePlayers)
            player.stop();
    for (auto& voicePlayers : virtualPlayers)
        for (auto& player : voicePlayers)
            player.stop();
}

void AudioEngine::reset() noexcept
{
    session.sequencer.reset();
    nextVoice.fill(0);
    nextVirtualVoice.fill(0);
    fragmentPlan = {};
    fragmentCursor = 0;
    fragmentFrame = 0;
    tracePlayer.reset();
}

bool AudioEngine::submitCommand(const EngineCommand& command) noexcept
{
    switch (command.type)
    {
        case EngineCommandType::setTempo:
            if (!std::isfinite(command.valueA) || command.valueA < 20.0
                || command.valueA > 400.0 || command.indexA > 3)
                return false;
            break;
        case EngineCommandType::selectPattern:
            if (command.indexA >= patternCount)
                return false;
            break;
        case EngineCommandType::selectSource:
            if (command.indexA > 1)
                return false;
            break;
        case EngineCommandType::setPatternCell:
            if (command.indexA >= patternCount || command.indexB >= stepsPerPattern
                || command.valueA < 0.0 || command.valueA > gapCellCode
                || std::floor(command.valueA) != command.valueA)
                return false;
            break;
        case EngineCommandType::togglePatternMemory:
            if (command.indexA >= patternCount
                || command.indexB >= stepsPerPattern)
                return false;
            break;
        case EngineCommandType::applyPatternTransform:
            if (command.indexA >= patternCount || command.indexB > 1
                || !std::isfinite(command.valueA)
                || !std::isfinite(command.valueB)
                || !std::isfinite(command.valueC)
                || command.valueA < 0.0 || command.valueA > 100.0
                || command.valueB < 0.0 || command.valueB > 100.0
                || command.valueC < 0.0 || command.valueC > 100.0
                || std::floor(command.valueA) != command.valueA
                || std::floor(command.valueB) != command.valueB
                || std::floor(command.valueC) != command.valueC)
                return false;
            break;
        case EngineCommandType::setMixerChannel:
            if (command.indexA > 1 || command.indexB > 3
                || !std::isfinite(command.valueA)
                || command.valueA < 0.0 || command.valueA > 1.25
                || !std::isfinite(command.valueB)
                || command.valueB < -1.0 || command.valueB > 1.0
                || !std::isfinite(command.valueC)
                || command.valueC < 0.0 || command.valueC > 2.0)
                return false;
            break;
        case EngineCommandType::setMixerBalance:
            if (!std::isfinite(command.valueA)
                || command.valueA < -1.0 || command.valueA > 1.0)
                return false;
            break;
        case EngineCommandType::divideSliceRegion:
            if (command.indexA > 1 || command.indexB == 0 || command.indexB > maxSlices
                || !Slice {command.valueA, command.valueB}.isValid())
                return false;
            break;
        case EngineCommandType::setSlice:
            if (command.indexA > 1 || command.indexB >= maxSlices
                || !Slice {command.valueA, command.valueB}.isValid())
                return false;
            break;
        case EngineCommandType::addBladeCut:
            if (command.indexA > 1 || !std::isfinite(command.valueA))
                return false;
            break;
        case EngineCommandType::undoBladeCut:
            if (command.indexA > 1)
                return false;
            break;
        case EngineCommandType::appendMicroSlices:
            if (command.indexA > 1 || command.indexB >= maxSlices
                || !std::isfinite(command.valueA)
                || command.valueA < 2.0 || command.valueA > 8.0
                || std::floor(command.valueA) != command.valueA
                || !std::isfinite(command.valueB) || command.valueB <= 0.0)
                return false;
            break;
        case EngineCommandType::setTiming:
            if (command.indexA > static_cast<std::size_t>(TimingMode::jitter)
                || !std::isfinite(command.valueA) || command.valueA < 0.0
                || command.valueA > 40.0 || !std::isfinite(command.valueB)
                || command.valueB < 0.0
                || command.valueB > static_cast<double>(
                    std::numeric_limits<std::uint32_t>::max())
                || std::floor(command.valueB) != command.valueB)
                return false;
            break;
        case EngineCommandType::setMasterLevel:
            if (!std::isfinite(command.valueA)
                || command.valueA < 0.0 || command.valueA > 1.0)
                return false;
            break;
        case EngineCommandType::setHeritagePitch:
            if (!std::isfinite(command.valueA) || std::floor(command.valueA) != command.valueA
                || command.valueA < -12.0 || command.valueA > 11.0
                || !std::isfinite(command.valueB)
                || command.valueB < 0.0 || command.valueB > 1.0)
                return false;
            break;
        case EngineCommandType::setVirtualVoiceProperty:
            if (command.indexA > 1
                || command.indexB > static_cast<std::size_t>(VirtualVoiceProperty::release)
                || !std::isfinite(command.valueA))
                return false;
            switch (static_cast<VirtualVoiceProperty>(command.indexB))
            {
                case VirtualVoiceProperty::enabled:
                case VirtualVoiceProperty::source:
                    if (command.valueA != 0.0 && command.valueA != 1.0)
                        return false;
                    break;
                case VirtualVoiceProperty::division:
                    if (command.valueA != 1.0 && command.valueA != 2.0
                        && command.valueA != 4.0 && command.valueA != 8.0)
                        return false;
                    break;
                case VirtualVoiceProperty::patternLength:
                    if (command.valueA < 1.0 || command.valueA > 16.0
                        || std::floor(command.valueA) != command.valueA)
                        return false;
                    break;
                case VirtualVoiceProperty::focusStart:
                case VirtualVoiceProperty::focusEnd:
                    if (command.valueA < 0.0 || command.valueA > 1.0)
                        return false;
                    break;
                case VirtualVoiceProperty::pitch:
                    if (command.valueA < -12.0 || command.valueA > 11.0
                        || std::floor(command.valueA) != command.valueA)
                        return false;
                    break;
                case VirtualVoiceProperty::level:
                    if (command.valueA < 0.0 || command.valueA > 0.8)
                        return false;
                    break;
                case VirtualVoiceProperty::pan:
                    if (command.valueA < -1.0 || command.valueA > 1.0)
                        return false;
                    break;
                case VirtualVoiceProperty::attack:
                    if (command.valueA < 0.001 || command.valueA > 0.5)
                        return false;
                    break;
                case VirtualVoiceProperty::release:
                    if (command.valueA < 0.005 || command.valueA > 1.5)
                        return false;
                    break;
            }
            break;
        case EngineCommandType::setVirtualVoicePatternCell:
            if (command.indexA > 1 || command.indexB >= 16
                || !std::isfinite(command.valueA) || command.valueA < 0.0
                || command.valueA > 127.0 || std::floor(command.valueA) != command.valueA)
                return false;
            break;
        case EngineCommandType::triggerSlice:
            if (command.indexA > 1 || command.indexB >= maxSlices
                || (command.valueA != 0.0 && command.valueA != 1.0))
                return false;
            break;
        case EngineCommandType::startStutter:
            if (!std::isfinite(command.valueA) || command.valueA < 0.0
                || command.valueA > gapCellCode
                || std::floor(command.valueA) != command.valueA)
                return false;
            break;
        case EngineCommandType::startBurst:
            if (command.indexA > 1)
                return false;
            break;
        case EngineCommandType::startTraceLoop:
            if (command.indexA > 1)
                return false;
            break;
        case EngineCommandType::appendControlTracePoint:
            if (!std::isfinite(command.valueA)
                || !std::isfinite(command.valueB)
                || !std::isfinite(command.valueC)
                || command.valueA < 0.0
                || command.valueA
                    > static_cast<double>(
                        std::numeric_limits<std::uint32_t>::max())
                || command.valueB < 20.0 || command.valueB > 400.0
                || command.valueC < -12.0 || command.valueC > 11.0
                || std::floor(command.valueA) != command.valueA
                || std::floor(command.valueB) != command.valueB
                || std::floor(command.valueC) != command.valueC
                || command.indexA > 1)
                return false;
            break;
        case EngineCommandType::setFormEnabled:
            if (command.indexA > 1)
                return false;
            break;
        case EngineCommandType::selectFormScene:
            if (command.indexA >= maxFormScenes)
                return false;
            break;
        case EngineCommandType::setFormSceneBasic:
            if (command.indexA >= maxFormScenes
                || !std::isfinite(command.valueA)
                || !std::isfinite(command.valueB)
                || !std::isfinite(command.valueC)
                || command.valueA < 1.0 || command.valueA > 128.0
                || command.valueB < 0.0 || command.valueB > 100.0
                || command.valueC < 0.0 || command.valueC > 100.0
                || std::floor(command.valueA) != command.valueA
                || std::floor(command.valueB) != command.valueB
                || std::floor(command.valueC) != command.valueC)
                return false;
            break;
        case EngineCommandType::setFormSceneProfiles:
            if (command.indexA >= maxFormScenes
                || (command.indexB & 0xffU) > 6
                || ((command.indexB >> 8U) & 0xffU) > 6
                || ((command.indexB >> 16U) & 0xffU) > 6
                || !std::isfinite(command.valueA)
                || !std::isfinite(command.valueB)
                || !std::isfinite(command.valueC)
                || command.valueA < 0.0 || command.valueA > 100.0
                || command.valueB < 0.0 || command.valueB > 100.0
                || command.valueC < 0.0 || command.valueC > 100.0)
                return false;
            break;
        case EngineCommandType::setFormSceneCharacter:
            if (command.indexA >= maxFormScenes
                || !std::isfinite(command.valueA)
                || !std::isfinite(command.valueB)
                || !std::isfinite(command.valueC)
                || command.valueA < 0.0 || command.valueA > 100.0
                || command.valueB < 0.0 || command.valueB > 100.0
                || command.valueC < 0.0 || command.valueC > 100.0)
                return false;
            break;
        case EngineCommandType::moveFormScene:
            if (command.valueA != -1.0 && command.valueA != 1.0)
                return false;
            break;
        case EngineCommandType::setAssistedSettings:
            if (command.indexA > 1023
                || !std::isfinite(command.valueA)
                || !std::isfinite(command.valueB)
                || !std::isfinite(command.valueC)
                || command.valueA < 20.0 || command.valueA > 400.0
                || command.valueB < 20.0 || command.valueB > 400.0
                || command.valueC < 0.0 || command.valueC > 100.0)
                return false;
            break;
        case EngineCommandType::setAssistedSeed:
            if (!std::isfinite(command.valueA) || command.valueA < 0.0
                || command.valueA > static_cast<double>(
                    std::numeric_limits<std::uint32_t>::max())
                || std::floor(command.valueA) != command.valueA)
                return false;
            break;
        default:
            break;
    }
    return commandQueue.push(command);
}

void AudioEngine::synchronizePendingCommands() noexcept
{
    applyPendingCommands();
}

StereoSample AudioEngine::processSample() noexcept
{
    applyPendingCommands();
    applyOutputControlTargets();
    const auto output = finalizeOutput(renderSample(), {});
    publishPeak(outputPeakLeft, std::abs(output.left));
    publishPeak(outputPeakRight, std::abs(output.right));
    publishOutputSafetyTelemetry(
        outputProfile == OutputProfile::liveSafe
            ? outputStage.consumeTelemetry() : OutputStageTelemetry {},
        std::abs(output.left), std::abs(output.right));
    publishTransportTelemetry();
    return output;
}

void AudioEngine::processBlock(float* leftOutput,
                               float* rightOutput,
                               std::size_t sampleCount,
                               const float* externalLeft,
                               const float* externalRight) noexcept
{
    if (leftOutput == nullptr || rightOutput == nullptr)
        return;

    applyPendingCommands();
    applyOutputControlTargets();
    float blockPeakLeft = 0.0F;
    float blockPeakRight = 0.0F;
    double blockSquareLeft = 0.0;
    double blockSquareRight = 0.0;
    for (std::size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        const StereoSample external {
            externalLeft != nullptr ? externalLeft[sampleIndex] : 0.0F,
            externalRight != nullptr ? externalRight[sampleIndex] : 0.0F
        };
        const auto output = finalizeOutput(renderSample(), external);
        leftOutput[sampleIndex] = output.left;
        rightOutput[sampleIndex] = output.right;
        blockPeakLeft = std::max(blockPeakLeft, std::abs(output.left));
        blockPeakRight = std::max(blockPeakRight, std::abs(output.right));
        blockSquareLeft += static_cast<double>(output.left) * output.left;
        blockSquareRight += static_cast<double>(output.right) * output.right;
    }
    publishPeak(outputPeakLeft, blockPeakLeft);
    publishPeak(outputPeakRight, blockPeakRight);
    const auto denominator = sampleCount > 0
        ? static_cast<double>(sampleCount) : 1.0;
    publishOutputSafetyTelemetry(
        outputProfile == OutputProfile::liveSafe
            ? outputStage.consumeTelemetry() : OutputStageTelemetry {},
        static_cast<float>(std::sqrt(blockSquareLeft / denominator)),
        static_cast<float>(std::sqrt(blockSquareRight / denominator)));
    publishTransportTelemetry();
}

bool AudioEngine::popRecordedFrame(StereoSample& sample) noexcept
{
    return recordingFifo.pop(sample);
}

std::uint64_t AudioEngine::droppedRecordingFrames() const noexcept
{
    return recordingFifo.dropped();
}

bool AudioEngine::isRecordingActive() const noexcept
{
    return recordingActive.load(std::memory_order_acquire);
}

StereoSample AudioEngine::consumeOutputPeak() noexcept
{
    return {
        outputPeakLeft.exchange(0.0F, std::memory_order_acq_rel),
        outputPeakRight.exchange(0.0F, std::memory_order_acq_rel)
    };
}

OutputSafetyTelemetry AudioEngine::consumeOutputSafetyTelemetry() noexcept
{
    return {
        {outputRmsLeft.exchange(0.0F, std::memory_order_acq_rel),
         outputRmsRight.exchange(0.0F, std::memory_order_acq_rel)},
        outputInputSamplePeak.exchange(0.0F, std::memory_order_acq_rel),
        outputInputTruePeak.exchange(0.0F, std::memory_order_acq_rel),
        outputTruePeak.exchange(0.0F, std::memory_order_acq_rel),
        outputGainReductionDb.exchange(0.0F, std::memory_order_acq_rel),
        outputNonFiniteSamples.exchange(0, std::memory_order_acq_rel),
        outputCeilingEngaged.exchange(false, std::memory_order_acq_rel),
        outputProfile == OutputProfile::liveSafe,
        requestedOutputTrimDb.load(std::memory_order_acquire),
        requestedOutputMuted.load(std::memory_order_acquire)
            || requestedOutputSuspended.load(std::memory_order_acquire),
        requestedOutputSuspended.load(std::memory_order_acquire)
    };
}

std::size_t AudioEngine::outputLatencySamples() const noexcept
{
    return outputProfile == OutputProfile::liveSafe
        ? outputStage.latencySamples() : 0;
}

void AudioEngine::publishOutputSafetyTelemetry(OutputStageTelemetry telemetry,
                                               float rmsLeft,
                                               float rmsRight) noexcept
{
    publishPeak(outputRmsLeft, rmsLeft);
    publishPeak(outputRmsRight, rmsRight);
    publishPeak(outputInputSamplePeak, telemetry.inputSamplePeak);
    publishPeak(outputInputTruePeak, telemetry.inputTruePeak);
    publishPeak(outputTruePeak, telemetry.outputTruePeak);
    publishPeak(outputGainReductionDb, telemetry.gainReductionDb);
    if (telemetry.nonFiniteSamples != 0)
        outputNonFiniteSamples.fetch_add(
            telemetry.nonFiniteSamples, std::memory_order_relaxed);
    if (telemetry.sampleCeilingLatched)
        outputCeilingEngaged.store(true, std::memory_order_release);
}

void AudioEngine::applyOutputControlTargets() noexcept
{
    const auto trimDb = requestedOutputTrimDb.load(std::memory_order_acquire);
    if (trimDb != appliedOutputTrimDb)
    {
        appliedOutputTrimDb = trimDb;
        outputStage.setOutputTrimDb(trimDb);
    }
    const auto shouldMute = requestedOutputMuted.load(std::memory_order_acquire)
        || requestedOutputSuspended.load(std::memory_order_acquire);
    if (shouldMute != appliedOutputMuted)
    {
        appliedOutputMuted = shouldMute;
        outputStage.setMuted(shouldMute);
    }
}

TransportTelemetry AudioEngine::transportTelemetry() const noexcept
{
    return {
        telemetryRunning.load(std::memory_order_acquire),
        telemetryStep.load(std::memory_order_acquire),
        telemetryGeneration.load(std::memory_order_acquire),
        telemetryFormScene.load(std::memory_order_relaxed),
        telemetryFormBar.load(std::memory_order_relaxed),
        telemetryFormCompleted.load(std::memory_order_relaxed),
        telemetryTracePlaying.load(std::memory_order_relaxed),
        telemetryTraceCycles.load(std::memory_order_relaxed),
        telemetryBpm.load(std::memory_order_relaxed),
        telemetryPitch.load(std::memory_order_relaxed),
        telemetryActiveSource.load(std::memory_order_relaxed),
        telemetryCurrentPattern.load(std::memory_order_relaxed),
        telemetryMixerBalance.load(std::memory_order_relaxed),
        {telemetryMixerPan[0].load(std::memory_order_relaxed),
         telemetryMixerPan[1].load(std::memory_order_relaxed)},
        {telemetryMixerWidth[0].load(std::memory_order_relaxed),
         telemetryMixerWidth[1].load(std::memory_order_relaxed)},
        {telemetrySourcePlayhead[0].load(std::memory_order_relaxed),
         telemetrySourcePlayhead[1].load(std::memory_order_relaxed)},
        {telemetryPatternRow[0].load(std::memory_order_relaxed),
         telemetryPatternRow[1].load(std::memory_order_relaxed),
         telemetryPatternRow[2].load(std::memory_order_relaxed),
         telemetryPatternRow[3].load(std::memory_order_relaxed),
         telemetryPatternRow[4].load(std::memory_order_relaxed),
         telemetryPatternRow[5].load(std::memory_order_relaxed),
         telemetryPatternRow[6].load(std::memory_order_relaxed),
         telemetryPatternRow[7].load(std::memory_order_acquire)}
    };
}

void AudioEngine::publishTransportTelemetry() noexcept
{
    telemetryStep.store(session.sequencer.currentStep(), std::memory_order_relaxed);
    telemetryGeneration.store(
        session.sequencer.generation(), std::memory_order_relaxed);
    telemetryRunning.store(
        session.sequencer.isRunning(), std::memory_order_release);
    const auto& form = session.formDirector.state();
    telemetryFormScene.store(form.currentScene, std::memory_order_relaxed);
    telemetryFormBar.store(form.bar, std::memory_order_relaxed);
    telemetryFormCompleted.store(form.completed, std::memory_order_relaxed);
    telemetryTracePlaying.store(
        tracePlayer.isPlaying(), std::memory_order_relaxed);
    telemetryTraceCycles.store(
        tracePlayer.cycleCount(), std::memory_order_relaxed);
    telemetryBpm.store(
        session.sequencer.tempo(), std::memory_order_relaxed);
    telemetryPitch.store(
        session.heritagePitchSemitones, std::memory_order_relaxed);
    telemetryActiveSource.store(
        session.activeSource, std::memory_order_relaxed);
    telemetryCurrentPattern.store(
        session.sequencer.currentPattern(), std::memory_order_relaxed);
    telemetryMixerBalance.store(
        session.mixer.balance, std::memory_order_relaxed);
    telemetryMixerPan[0].store(
        session.mixer.sourceA.pan, std::memory_order_relaxed);
    telemetryMixerPan[1].store(
        session.mixer.sourceB.pan, std::memory_order_relaxed);
    telemetryMixerWidth[0].store(
        session.mixer.sourceA.width, std::memory_order_relaxed);
    telemetryMixerWidth[1].store(
        session.mixer.sourceB.width, std::memory_order_relaxed);
    for (std::size_t source = 0; source < players.size(); ++source)
    {
        auto playhead = -1.0;
        for (std::size_t offset = 0; offset < voicesPerSource; ++offset)
        {
            const auto voice = (nextVoice[source] + voicesPerSource - 1 - offset)
                % voicesPerSource;
            if (players[source][voice].isPlaying())
            {
                playhead = players[source][voice].normalizedPosition();
                break;
            }
        }
        telemetrySourcePlayhead[source].store(
            playhead, std::memory_order_relaxed);
    }
    const auto& currentPattern = session.patterns.pattern(
        session.sequencer.currentPattern());
    for (std::size_t step = 0; step < stepsPerPattern; ++step)
        telemetryPatternRow[step].store(
            currentPattern[step],
            step + 1 == stepsPerPattern
                ? std::memory_order_release : std::memory_order_relaxed);
}

StereoSample AudioEngine::renderSample() noexcept
{
    advanceControlTrace();
    advanceFragmentGesture();
    if (const auto event = session.sequencer.processSample())
    {
        triggerVirtualVoices(event->step);
        trigger(*event);
        if (event->step + 1 == stepsPerPattern)
        {
            applyAssistedPhrase(event->step);
            const auto previousScene =
                session.formDirector.state().currentScene;
            static_cast<void>(
                session.formDirector.notePhraseCompleted());
            if (session.formDirector.state().currentScene != previousScene)
                session.applyCurrentFormSceneMaterial();
        }
    }

    auto output = mixer.process(renderSource(0), renderSource(1));
    for (std::size_t voiceIndex = 0; voiceIndex < virtualPlayers.size(); ++voiceIndex)
    {
        StereoSample voiceOutput;
        for (auto& player : virtualPlayers[voiceIndex])
        {
            const auto sample = player.process();
            voiceOutput.left += sample.left;
            voiceOutput.right += sample.right;
        }
        const auto mixed = virtualMixers[voiceIndex].process(voiceOutput);
        output.left += mixed.left;
        output.right += mixed.right;
    }
    output = pitch.process(output);
    const auto gain = masterLevel.next();
    output.left *= gain;
    output.right *= gain;
    return output;
}

StereoSample AudioEngine::finalizeOutput(StereoSample program,
                                         StereoSample external) noexcept
{
    StereoSample output {
        program.left + external.left,
        program.right + external.right
    };
    if (outputProfile == OutputProfile::liveSafe)
        output = outputStage.process(output);
    if (recording)
        static_cast<void>(recordingFifo.push(output));
    return output;
}

void AudioEngine::applyAssistedPhrase(std::size_t step) noexcept
{
    if (!session.assisted.enabled
        || (!session.sources[0].hasAudio && !session.sources[1].hasAudio))
        return;
    if (session.assisted.repeat && assistedRepeatRemaining != 0)
    {
        --assistedRepeatRemaining;
        return;
    }
    auto effectiveSettings = session.assisted;
    effectiveSettings.chooseSource =
        effectiveSettings.chooseSource && !session.motifLocks.source;
    effectiveSettings.changeOrder =
        effectiveSettings.changeOrder && !session.motifLocks.pattern;
    effectiveSettings.editRegion =
        effectiveSettings.editRegion && !session.motifLocks.cuts;
    effectiveSettings.editSlices =
        effectiveSettings.editSlices && !session.motifLocks.cuts;
    effectiveSettings.autoMix =
        effectiveSettings.autoMix && !session.motifLocks.mix;
    effectiveSettings.applyTransform =
        effectiveSettings.applyTransform && !session.motifLocks.transform;
    effectiveSettings.useGaps =
        effectiveSettings.useGaps && !session.motifLocks.gap;
    effectiveSettings.changePitch =
        effectiveSettings.changePitch && !session.motifLocks.pitch;
    AssistedPhraseInput input;
    input.currentBpm =
        static_cast<int>(std::lround(session.sequencer.tempo()));
    input.currentSource = session.activeSource;
    input.currentPattern = session.sequencer.currentPattern();
    input.playable = {
        session.sources[0].hasAudio, session.sources[1].hasAudio};
    input.sliceCounts = {
        session.sources[0].sliceBank.size(),
        session.sources[1].sliceBank.size()};
    for (std::size_t pattern = 0; pattern < patternCount; ++pattern)
    {
        input.patterns[pattern] = session.patterns.pattern(pattern);
        input.memory[pattern] = session.patternMemory[pattern];
    }
    for (std::size_t source = 0; source < 2; ++source)
    {
        const auto slices = session.sources[source].sliceBank.slices();
        input.regions[source] = {
            slices.front().start, slices.back().end};
    }
    input.mixer.balance = session.mixer.balance;
    input.mixer.pan = {
        session.mixer.sourceA.pan, session.mixer.sourceB.pan};
    input.mixer.width = {
        session.mixer.sourceA.width, session.mixer.sourceB.width};
    input.mixer.available = input.playable[0] && input.playable[1]
        && !session.mixer.sourceA.muted && !session.mixer.sourceB.muted
        && !session.mixer.sourceA.solo && !session.mixer.sourceB.solo;
    const auto decision = planAssistedPhrase(
        effectiveSettings, session.formDirector.state(),
        input, session.assistedRng);
    if (decision.switchesSource)
        session.selectSource(decision.source);
    if (decision.selectsPattern)
    {
        session.commitPatternTransform();
        session.sequencer.selectPattern(decision.pattern);
    }
    if (decision.changesPattern)
    {
        session.commitPatternTransform();
        session.patterns.setPattern(decision.pattern, decision.patternRow);
    }
    if ((decision.editsRegion
         || decision.cutAction != AssistedCutAction::none)
        && !hasAssistedCutBase)
    {
        assistedCutBase = {
            session.sources[0].sliceBank,
            session.sources[1].sliceBank};
        hasAssistedCutBase = true;
    }
    if (decision.editsRegion)
        session.sources[decision.regionSource].sliceBank.divideRegion(
            decision.region.start, decision.region.end,
            decision.regionDivision);
    if (decision.cutAction != AssistedCutAction::none)
    {
        auto& bank = session.sources[decision.cutSource].sliceBank;
        if (decision.cutAction == AssistedCutAction::nudge)
        {
            const auto index = std::min(
                decision.cutIndex, bank.size() - 1);
            const auto pair = bank.slices()[index];
            const auto reach =
                (pair.end - pair.start)
                * (0.04 + decision.context.intensity * 0.08);
            const auto delta = (decision.cutValue * 2.0 - 1.0) * reach;
            const auto start = std::clamp(pair.start + delta, 0.0, 0.999);
            const auto end = std::clamp(
                pair.end + delta, start + 0.00001, 1.0);
            bank.setSlice(index, {start, end});
        }
        else if (decision.cutAction == AssistedCutAction::micro)
        {
            const auto* buffer = sourceBuffers[decision.cutSource];
            if (buffer != nullptr)
                static_cast<void>(bank.appendMicroSlices(
                    std::min(decision.cutIndex, bank.size() - 1),
                    decision.cutCount,
                    static_cast<double>(buffer->size())
                        / buffer->sampleRate()));
        }
        else if (decision.cutAction == AssistedCutAction::blade)
        {
            const auto slices = bank.slices();
            const auto start = slices.front().start;
            const auto end = slices.back().end;
            static_cast<void>(bank.addBladeCut(
                start + (end - start) * decision.cutValue));
        }
        else if (decision.cutAction == AssistedCutAction::undo)
            static_cast<void>(bank.undoBladeCut());
        else if (decision.cutAction == AssistedCutAction::redivide)
        {
            const auto slices = bank.slices();
            bank.divideRegion(
                slices.front().start, slices.back().end,
                std::min<std::size_t>(
                    64, std::max<std::size_t>(4, bank.size() * 2)));
        }
    }
    session.sequencer.setTempo(
        decision.bpm, session.sequencer.division());
    if (decision.transforms)
    {
        session.commitPatternTransform();
        session.applyPatternTransform(
            session.sequencer.currentPattern(),
            decision.transform, session.assisted.useGaps);
    }
    if (decision.changesPitch)
    {
        session.heritagePitchSemitones = decision.pitch;
        session.heritagePitchMode = 1.0;
        pitch.setSemitones(decision.pitch);
        pitch.setMode(1.0F);
    }
    if (decision.mixes)
    {
        session.mixer.balance = decision.mixer.balance;
        session.mixer.sourceA.pan = decision.mixer.pan[0];
        session.mixer.sourceB.pan = decision.mixer.pan[1];
        session.mixer.sourceA.width = decision.mixer.width[0];
        session.mixer.sourceB.width = decision.mixer.width[1];
        syncMixerParameters();
    }
    const auto code = session.patterns.cell(
        session.sequencer.currentPattern(), step);
    if (decision.fragment == AssistedFragment::stutter)
    {
        fragmentPlan = planStutter(
            code, session.sequencer.tempo(), session.sequencer.division(),
            outputSampleRate);
        fragmentCursor = 0;
        fragmentFrame = 0;
    }
    else if (decision.fragment == AssistedFragment::burst)
    {
        fragmentPlan = planBurst(
            session.patterns.pattern(session.sequencer.currentPattern()),
            session.activeSource,
            {session.sources[0].sliceBank.size(),
             session.sources[1].sliceBank.size()},
            session.sequencer.tempo(), session.sequencer.division(),
            outputSampleRate, session.assistedRng);
        fragmentCursor = 0;
        fragmentFrame = 0;
    }
    else if (decision.fragment == AssistedFragment::reverse)
    {
        const auto cell = PatternCell::decode(code);
        if (cell.kind != PatternCell::Kind::gap)
            triggerSlice(
                cell.kind == PatternCell::Kind::sourceA ? 0U : 1U,
                cell.sliceIndex, true);
    }
    if (session.assisted.repeat)
    {
        const auto intensity = std::clamp(decision.context.intensity, 0.0, 1.0);
        const auto maximumRepeats = std::max(
            1, static_cast<int>(std::lround(3.0 - intensity * 2.0)));
        assistedRepeatRemaining = static_cast<std::size_t>(
            std::floor(session.assistedRng.next()
                       * static_cast<double>(maximumRepeats))) + 1U;
    }
    else
        assistedRepeatRemaining = 0;
}

void AudioEngine::advanceControlTrace() noexcept
{
    const auto* point = tracePlayer.advance();
    if (point == nullptr)
        return;
    session.sequencer.setTempo(
        static_cast<double>(point->bpm), session.sequencer.division());
    session.heritagePitchSemitones = point->pitch;
    session.heritagePitchMode = 1.0;
    pitch.setSemitones(point->pitch);
    pitch.setMode(1.0F);
}

void AudioEngine::advanceFragmentGesture() noexcept
{
    while (fragmentCursor < fragmentPlan.count
           && fragmentPlan.frameOffsets[fragmentCursor] <= fragmentFrame)
        triggerFragmentCell(fragmentPlan.cells[fragmentCursor++]);
    if (fragmentCursor < fragmentPlan.count)
        ++fragmentFrame;
}

void AudioEngine::triggerFragmentCell(std::uint16_t code) noexcept
{
    const auto cell = PatternCell::decode(code);
    if (cell.kind == PatternCell::Kind::gap)
    {
        for (auto& sourcePlayers : players)
            for (auto& player : sourcePlayers)
                player.stop();
        return;
    }
    triggerSlice(
        cell.kind == PatternCell::Kind::sourceA ? 0U : 1U,
        cell.sliceIndex, false);
}

void AudioEngine::triggerVirtualVoices(std::size_t step) noexcept
{
    const auto enabledCount = static_cast<std::size_t>(session.virtualVoices[0].enabled)
        + static_cast<std::size_t>(session.virtualVoices[1].enabled);
    const auto safety = enabledCount > 1 ? 0.70 : 0.86;

    for (std::size_t voiceIndex = 0; voiceIndex < session.virtualVoices.size(); ++voiceIndex)
    {
        const auto& state = session.virtualVoices[voiceIndex];
        if (!state.enabled || state.sourceIndex > 1 || state.division == 0
            || step % state.division != 0 || !session.sources[state.sourceIndex].hasAudio)
            continue;

        const auto slices = session.sources[state.sourceIndex].sliceBank.slices();
        if (slices.empty() || state.patternLength == 0)
            continue;
        const auto focusStart = std::clamp(state.focusStart, 0.0, 0.99);
        const auto focusEnd = std::clamp(state.focusEnd, focusStart + 0.01, 1.0);
        const auto low = std::min(
            slices.size() - 1,
            static_cast<std::size_t>(std::floor(focusStart * slices.size())));
        const auto highExclusive = std::max<std::size_t>(
            1, std::min(slices.size(),
                        static_cast<std::size_t>(std::ceil(focusEnd * slices.size()))));
        const auto high = std::clamp(highExclusive - 1, low, slices.size() - 1);
        const auto span = high - low + 1;
        const auto patternLength = std::min(state.patternLength, state.pattern.size());
        const auto patternPosition = (step / state.division) % patternLength;
        const auto sliceIndex = low + state.pattern[patternPosition] % span;

        auto& slots = virtualPlayers[voiceIndex];
        slots[nextVirtualVoice[voiceIndex]].stop();
        nextVirtualVoice[voiceIndex] = (nextVirtualVoice[voiceIndex] + 1) % voicesPerSource;
        const auto semitones = std::clamp(state.pitchSemitones, -12, 11);
        const auto rate = std::pow(2.0, static_cast<double>(semitones) / 12.0);
        auto& newVoice = slots[nextVirtualVoice[voiceIndex]];
        newVoice.setBuffer(sourceBuffers[state.sourceIndex]);
        static_cast<void>(newVoice.tryTrigger(
            slices[sliceIndex], false, rate, state.attackSeconds, state.releaseSeconds));
        virtualMixers[voiceIndex].reset(
            static_cast<float>(std::clamp(state.level * safety, 0.0, 0.65)),
            static_cast<float>(std::clamp(state.pan, -1.0, 1.0)),
            1.0F);
    }
}

void AudioEngine::trigger(const SequencerEvent& event) noexcept
{
    if (event.cell.kind == PatternCell::Kind::gap)
        return;

    const auto sourceIndex = event.cell.kind == PatternCell::Kind::sourceA ? 0U : 1U;
    const auto sliceIndex = static_cast<std::size_t>(event.cell.sliceIndex);
    triggerSlice(sourceIndex, sliceIndex, false);
}

void AudioEngine::triggerSlice(std::size_t sourceIndex,
                               std::size_t sliceIndex,
                               bool reverse) noexcept
{
    if (sourceIndex >= players.size())
        return;
    const auto& slices = session.sources[sourceIndex].sliceBank.slices();
    if (!session.sources[sourceIndex].hasAudio || sliceIndex >= slices.size())
        return;

    auto& sourcePlayers = players[sourceIndex];
    auto& voice = sourcePlayers[nextVoice[sourceIndex]];
    voice.stop();
    nextVoice[sourceIndex] = (nextVoice[sourceIndex] + 1) % voicesPerSource;

    auto& newVoice = sourcePlayers[nextVoice[sourceIndex]];
    static_cast<void>(newVoice.tryTrigger(slices[sliceIndex], reverse));
}

void AudioEngine::applyPendingCommands() noexcept
{
    EngineCommand command;
    while (commandQueue.pop(command))
        applyCommand(command);
}

void AudioEngine::applyCommand(const EngineCommand& command) noexcept
{
    switch (command.type)
    {
        case EngineCommandType::start:
            start();
            break;
        case EngineCommandType::stop:
            stop();
            break;
        case EngineCommandType::reset:
            reset();
            break;
        case EngineCommandType::setTempo:
            session.sequencer.setTempo(command.valueA, command.indexA);
            break;
        case EngineCommandType::selectPattern:
            session.sequencer.selectPattern(command.indexA);
            break;
        case EngineCommandType::selectSource:
            session.selectSource(command.indexA);
            break;
        case EngineCommandType::setPatternCell:
            session.patterns.setCell(
                command.indexA, command.indexB, static_cast<std::uint16_t>(command.valueA));
            break;
        case EngineCommandType::togglePatternMemory:
            static_cast<void>(
                session.togglePatternMemory(command.indexA, command.indexB));
            break;
        case EngineCommandType::applyPatternTransform:
            session.applyPatternTransform(
                command.indexA,
                {static_cast<int>(command.valueA),
                 static_cast<int>(command.valueB),
                 static_cast<int>(command.valueC)},
                command.indexB != 0);
            break;
        case EngineCommandType::commitPatternTransform:
            session.commitPatternTransform();
            break;
        case EngineCommandType::restorePatternTransform:
            session.restorePatternTransform();
            break;
        case EngineCommandType::setMixerChannel:
        {
            auto& channel = command.indexA == 0
                ? session.mixer.sourceA : session.mixer.sourceB;
            channel.level = command.valueA;
            channel.pan = command.valueB;
            channel.width = command.valueC;
            channel.muted = (command.indexB & 1U) != 0;
            channel.solo = (command.indexB & 2U) != 0;
            syncMixerParameters();
            break;
        }
        case EngineCommandType::setMixerBalance:
            session.mixer.balance = command.valueA;
            syncMixerParameters();
            break;
        case EngineCommandType::divideSliceRegion:
            session.sources[command.indexA].sliceBank.divideRegion(
                command.valueA, command.valueB, command.indexB);
            break;
        case EngineCommandType::setSlice:
            if (command.indexB < session.sources[command.indexA].sliceBank.size())
                session.sources[command.indexA].sliceBank.setSlice(
                    command.indexB, {command.valueA, command.valueB});
            break;
        case EngineCommandType::addBladeCut:
            static_cast<void>(
                session.sources[command.indexA].sliceBank.addBladeCut(command.valueA));
            break;
        case EngineCommandType::undoBladeCut:
            static_cast<void>(session.sources[command.indexA].sliceBank.undoBladeCut());
            break;
        case EngineCommandType::appendMicroSlices:
            if (command.indexB
                < session.sources[command.indexA].sliceBank.size())
                static_cast<void>(
                    session.sources[command.indexA].sliceBank.appendMicroSlices(
                        command.indexB,
                        static_cast<std::size_t>(command.valueA),
                        command.valueB));
            break;
        case EngineCommandType::setTiming:
            session.sequencer.setTiming(
                static_cast<TimingMode>(command.indexA),
                command.valueA,
                static_cast<std::uint32_t>(command.valueB));
            break;
        case EngineCommandType::setMasterLevel:
            session.masterLevel = command.valueA;
            masterLevel.setTarget(static_cast<float>(command.valueA));
            break;
        case EngineCommandType::setHeritagePitch:
            session.heritagePitchSemitones = static_cast<int>(command.valueA);
            session.heritagePitchMode = command.valueB;
            pitch.setSemitones(session.heritagePitchSemitones);
            pitch.setMode(static_cast<float>(session.heritagePitchMode));
            break;
        case EngineCommandType::setVirtualVoiceProperty:
        {
            auto& voice = session.virtualVoices[command.indexA];
            switch (static_cast<VirtualVoiceProperty>(command.indexB))
            {
                case VirtualVoiceProperty::enabled:
                    voice.enabled = command.valueA != 0.0;
                    break;
                case VirtualVoiceProperty::source:
                    voice.sourceIndex = static_cast<std::size_t>(command.valueA);
                    break;
                case VirtualVoiceProperty::division:
                    voice.division = static_cast<std::size_t>(command.valueA);
                    break;
                case VirtualVoiceProperty::patternLength:
                    voice.patternLength = static_cast<std::size_t>(command.valueA);
                    break;
                case VirtualVoiceProperty::focusStart:
                    voice.focusStart = command.valueA;
                    break;
                case VirtualVoiceProperty::focusEnd:
                    voice.focusEnd = command.valueA;
                    break;
                case VirtualVoiceProperty::pitch:
                    voice.pitchSemitones = static_cast<int>(command.valueA);
                    break;
                case VirtualVoiceProperty::level:
                    voice.level = command.valueA;
                    break;
                case VirtualVoiceProperty::pan:
                    voice.pan = command.valueA;
                    break;
                case VirtualVoiceProperty::attack:
                    voice.attackSeconds = command.valueA;
                    break;
                case VirtualVoiceProperty::release:
                    voice.releaseSeconds = command.valueA;
                    break;
            }
            break;
        }
        case EngineCommandType::setVirtualVoicePatternCell:
            session.virtualVoices[command.indexA].pattern[command.indexB]
                = static_cast<std::uint8_t>(command.valueA);
            break;
        case EngineCommandType::triggerSlice:
            triggerSlice(command.indexA, command.indexB, command.valueA != 0.0);
            break;
        case EngineCommandType::startStutter:
            fragmentPlan = planStutter(
                static_cast<std::uint16_t>(command.valueA),
                session.sequencer.tempo(), session.sequencer.division(),
                outputSampleRate);
            fragmentCursor = 0;
            fragmentFrame = 0;
            break;
        case EngineCommandType::startBurst:
            fragmentPlan = planBurst(
                session.patterns.pattern(session.sequencer.currentPattern()),
                command.indexA,
                {session.sources[0].sliceBank.size(),
                 session.sources[1].sliceBank.size()},
                session.sequencer.tempo(), session.sequencer.division(),
                outputSampleRate, session.assistedRng);
            fragmentCursor = 0;
            fragmentFrame = 0;
            break;
        case EngineCommandType::startTraceLoop:
            static_cast<void>(
                tracePlayer.start(session.controlTrace, command.indexA != 0));
            break;
        case EngineCommandType::stopTraceLoop:
            tracePlayer.stop();
            break;
        case EngineCommandType::clearControlTrace:
            tracePlayer.stop();
            session.controlTrace.clear();
            break;
        case EngineCommandType::appendControlTracePoint:
            static_cast<void>(session.controlTrace.append(
                static_cast<std::uint32_t>(command.valueA),
                static_cast<int>(command.valueB),
                static_cast<int>(command.valueC),
                command.indexA != 0));
            break;
        case EngineCommandType::setFormEnabled:
            session.formDirector.setEnabled(command.indexA != 0);
            if (command.indexA != 0)
                session.applyCurrentFormSceneMaterial();
            break;
        case EngineCommandType::selectFormScene:
            if (session.formDirector.selectScene(command.indexA))
                session.applyCurrentFormSceneMaterial();
            break;
        case EngineCommandType::toggleFormHold:
            session.formDirector.toggleHold();
            break;
        case EngineCommandType::advanceFormScene:
            static_cast<void>(session.formDirector.advanceScene());
            session.applyCurrentFormSceneMaterial();
            break;
        case EngineCommandType::resetFormDirector:
            session.formDirector.reset();
            session.applyCurrentFormSceneMaterial();
            break;
        case EngineCommandType::setFormSceneBasic:
        {
            const auto& state = session.formDirector.state();
            if (command.indexA >= state.sceneCount)
                break;
            auto scene = state.scenes[command.indexA];
            scene.bars = static_cast<int>(command.valueA);
            scene.energy = static_cast<int>(command.valueB);
            scene.variation = static_cast<int>(command.valueC);
            static_cast<void>(
                session.formDirector.replaceCurrentScene(std::move(scene)));
            break;
        }
        case EngineCommandType::setFormSceneProfiles:
        {
            const auto& state = session.formDirector.state();
            if (command.indexA >= state.sceneCount)
                break;
            auto scene = state.scenes[command.indexA];
            scene.transition =
                static_cast<FormTransition>(command.indexB & 0xffU);
            scene.bankA =
                static_cast<SliceBankProfile>((command.indexB >> 8U) & 0xffU);
            scene.bankB =
                static_cast<SliceBankProfile>((command.indexB >> 16U) & 0xffU);
            scene.density = static_cast<int>(command.valueA);
            scene.tension = static_cast<int>(command.valueB);
            scene.stability = static_cast<int>(command.valueC);
            if (session.formDirector.replaceCurrentScene(std::move(scene)))
                session.applyCurrentFormSceneMaterial();
            break;
        }
        case EngineCommandType::setFormSceneCharacter:
        {
            const auto& state = session.formDirector.state();
            if (command.indexA >= state.sceneCount)
                break;
            auto scene = state.scenes[command.indexA];
            scene.continuity = static_cast<int>(command.valueA);
            scene.contrast = static_cast<int>(command.valueB);
            scene.stereoMotion = static_cast<int>(command.valueC);
            static_cast<void>(
                session.formDirector.replaceCurrentScene(std::move(scene)));
            break;
        }
        case EngineCommandType::toggleFormSceneLock:
            static_cast<void>(session.formDirector.toggleCurrentLock());
            break;
        case EngineCommandType::addFormScene:
            static_cast<void>(session.formDirector.addScene());
            break;
        case EngineCommandType::duplicateFormScene:
            static_cast<void>(session.formDirector.duplicateScene());
            break;
        case EngineCommandType::deleteFormScene:
            static_cast<void>(session.formDirector.deleteScene());
            break;
        case EngineCommandType::moveFormScene:
            static_cast<void>(
                session.formDirector.moveScene(static_cast<int>(command.valueA)));
            break;
        case EngineCommandType::setAssistedSettings:
            session.assisted.enabled = (command.indexA & 1U) != 0;
            session.assisted.repeat = (command.indexA & 1024U) != 0;
            session.assisted.chooseSource = (command.indexA & 2U) != 0;
            session.assisted.changeOrder = (command.indexA & 4U) != 0;
            session.assisted.editRegion = (command.indexA & 8U) != 0;
            session.assisted.editSlices = (command.indexA & 16U) != 0;
            session.assisted.autoMix = (command.indexA & 32U) != 0;
            session.assisted.applyTransform = (command.indexA & 64U) != 0;
            session.assisted.useGaps = (command.indexA & 128U) != 0;
            session.assisted.changePitch = (command.indexA & 256U) != 0;
            session.assisted.useFragments = (command.indexA & 512U) != 0;
            session.assisted.minBpm = static_cast<int>(command.valueA);
            session.assisted.maxBpm = static_cast<int>(command.valueB);
            session.assisted.variation = static_cast<int>(command.valueC);
            normalizeAssistedSettings(session.assisted);
            if (!session.assisted.enabled || !session.assisted.repeat)
                assistedRepeatRemaining = 0;
            break;
        case EngineCommandType::setAssistedSeed:
            session.assistedRng.setSeed(
                static_cast<std::uint32_t>(command.valueA));
            assistedRepeatRemaining = 0;
            break;
        case EngineCommandType::forceAssistedDecision:
            assistedRepeatRemaining = 0;
            applyAssistedPhrase(session.sequencer.currentStep());
            break;
        case EngineCommandType::keepAssistedCuts:
            hasAssistedCutBase = false;
            break;
        case EngineCommandType::restoreAssistedCuts:
            if (hasAssistedCutBase)
            {
                session.sources[0].sliceBank = assistedCutBase[0];
                session.sources[1].sliceBank = assistedCutBase[1];
                hasAssistedCutBase = false;
            }
            break;
        case EngineCommandType::setMotifLocks:
            session.motifLocks = {
                (command.indexA & 1U) != 0,
                (command.indexA & 2U) != 0,
                (command.indexA & 4U) != 0,
                (command.indexA & 8U) != 0,
                (command.indexA & 16U) != 0,
                (command.indexA & 32U) != 0,
                (command.indexA & 64U) != 0,
                (command.indexA & 128U) != 0};
            break;
        case EngineCommandType::startRecording:
            recordingFifo.resetDropped();
            recording = true;
            recordingActive.store(true, std::memory_order_release);
            break;
        case EngineCommandType::stopRecording:
            recording = false;
            recordingActive.store(false, std::memory_order_release);
            break;
    }
}

StereoSample AudioEngine::renderSource(std::size_t sourceIndex) noexcept
{
    StereoSample output;
    for (auto& player : players[sourceIndex])
    {
        const auto sample = player.process();
        output.left += sample.left;
        output.right += sample.right;
    }
    return output;
}
}
