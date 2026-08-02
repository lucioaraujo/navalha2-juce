#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "core/EngineCommandQueue.h"
#include "core/FragmentGesture.h"
#include "core/HeritagePitch.h"
#include "core/RecordingFifo.h"
#include "core/SessionModel.h"
#include "core/SlicePlayer.h"
#include "core/StereoMixer.h"

namespace navalha
{
struct TransportTelemetry
{
    bool running = false;
    std::size_t step = 0;
    std::uint64_t generation = 0;
    std::size_t formScene = 0;
    int formBar = 0;
    bool formCompleted = false;
    bool tracePlaying = false;
    std::size_t traceCycles = 0;
    double bpm = 120.0;
    int pitch = 0;
    std::size_t activeSource = 0;
    std::size_t currentPattern = 0;
    double mixerBalance = 0.0;
    std::array<double, 2> mixerPan {};
    std::array<double, 2> mixerWidth {1.0, 1.0};
    Pattern patternRow {};
};

class AudioEngine
{
public:
    explicit AudioEngine(SessionModel& sessionModel) noexcept;

    void prepare(double sampleRate);
    void setSourceBuffer(std::size_t sourceIndex, const StereoAudioBuffer* buffer);
    void syncMixerParameters();
    void start() noexcept;
    void stop() noexcept;
    void reset() noexcept;
    [[nodiscard]] bool submitCommand(const EngineCommand& command) noexcept;
    // Call only after the host audio callback has been stopped.
    void synchronizePendingCommands() noexcept;

    [[nodiscard]] StereoSample processSample() noexcept;
    void processBlock(float* leftOutput, float* rightOutput, std::size_t sampleCount) noexcept;
    [[nodiscard]] bool popRecordedFrame(StereoSample& sample) noexcept;
    [[nodiscard]] std::uint64_t droppedRecordingFrames() const noexcept;
    [[nodiscard]] bool isRecordingActive() const noexcept;
    [[nodiscard]] StereoSample consumeOutputPeak() noexcept;
    [[nodiscard]] TransportTelemetry transportTelemetry() const noexcept;

private:
    static constexpr std::size_t voicesPerSource = 2;

    void trigger(const SequencerEvent& event) noexcept;
    void triggerSlice(std::size_t sourceIndex,
                      std::size_t sliceIndex,
                      bool reverse) noexcept;
    void advanceFragmentGesture() noexcept;
    void advanceControlTrace() noexcept;
    void applyAssistedPhrase(std::size_t step) noexcept;
    void triggerFragmentCell(std::uint16_t code) noexcept;
    void triggerVirtualVoices(std::size_t step) noexcept;
    void applyPendingCommands() noexcept;
    void applyCommand(const EngineCommand& command) noexcept;
    void publishTransportTelemetry() noexcept;
    [[nodiscard]] StereoSample renderSample() noexcept;
    [[nodiscard]] StereoSample renderSource(std::size_t sourceIndex) noexcept;

    SessionModel& session;
    std::array<const StereoAudioBuffer*, 2> sourceBuffers {};
    std::array<std::array<SlicePlayer, voicesPerSource>, 2> players;
    std::array<std::size_t, 2> nextVoice {};
    std::array<std::array<SlicePlayer, voicesPerSource>, 2> virtualPlayers;
    std::array<std::size_t, 2> nextVirtualVoice {};
    std::array<StereoChannelProcessor, 2> virtualMixers;
    StereoSourceMixer mixer;
    HeritagePitch pitch;
    LinearRamp masterLevel;
    RecordingFifo<65536> recordingFifo;
    bool recording = false;
    std::atomic<bool> recordingActive {false};
    std::atomic<float> outputPeakLeft {0.0F};
    std::atomic<float> outputPeakRight {0.0F};
    std::atomic<bool> telemetryRunning {false};
    std::atomic<std::size_t> telemetryStep {0};
    std::atomic<std::uint64_t> telemetryGeneration {0};
    std::atomic<std::size_t> telemetryFormScene {0};
    std::atomic<int> telemetryFormBar {0};
    std::atomic<bool> telemetryFormCompleted {false};
    std::atomic<bool> telemetryTracePlaying {false};
    std::atomic<std::size_t> telemetryTraceCycles {0};
    std::atomic<double> telemetryBpm {120.0};
    std::atomic<int> telemetryPitch {0};
    std::atomic<std::size_t> telemetryActiveSource {0};
    std::atomic<std::size_t> telemetryCurrentPattern {0};
    std::atomic<double> telemetryMixerBalance {0.0};
    std::array<std::atomic<double>, 2> telemetryMixerPan {};
    std::array<std::atomic<double>, 2> telemetryMixerWidth {
        std::atomic<double> {1.0}, std::atomic<double> {1.0}};
    std::array<std::atomic<std::uint16_t>, stepsPerPattern> telemetryPatternRow {};
    EngineCommandQueue<256> commandQueue;
    FragmentGesturePlan fragmentPlan;
    ControlTracePlayer tracePlayer;
    std::array<SliceBank, 2> assistedCutBase;
    bool hasAssistedCutBase = false;
    std::size_t assistedRepeatRemaining = 0;
    std::size_t fragmentCursor = 0;
    std::uint64_t fragmentFrame = 0;
    double outputSampleRate = 44100.0;
};
}
