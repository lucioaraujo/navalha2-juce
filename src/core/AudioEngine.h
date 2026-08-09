#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "core/EngineCommandQueue.h"
#include "core/FragmentGesture.h"
#include "core/HeritagePitch.h"
#include "core/OutputStage.h"
#include "core/RecordingFifo.h"
#include "core/SessionModel.h"
#include "core/SlicePlayer.h"
#include "core/StereoMixer.h"

namespace navalha
{
enum class OutputProfile
{
    legacy,
    liveSafe
};

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
    std::array<double, 2> sourcePlayhead {-1.0, -1.0};
    Pattern patternRow {};
};

struct OutputSafetyTelemetry
{
    StereoSample rms;
    float inputSamplePeak = 0.0F;
    float inputTruePeak = 0.0F;
    float outputTruePeak = 0.0F;
    float gainReductionDb = 0.0F;
    std::uint64_t nonFiniteSamples = 0;
    bool ceilingEngaged = false;
    bool liveSafe = false;
    float outputTrimDb = 0.0F;
    bool muted = false;
    bool suspended = false;
};

class AudioEngine
{
public:
    explicit AudioEngine(SessionModel& sessionModel) noexcept;

    void prepare(double sampleRate);
    // Change only while the host audio callback is stopped, then call prepare.
    void setOutputProfile(OutputProfile profile) noexcept;
    [[nodiscard]] bool setOutputTrimDb(float trimDb) noexcept;
    void setOutputMuted(bool shouldMute) noexcept;
    void suspendOutput() noexcept;
    void resumeOutput() noexcept;
    void setSourceBuffer(std::size_t sourceIndex, const StereoAudioBuffer* buffer);
    void syncMixerParameters();
    void start() noexcept;
    void stop() noexcept;
    void reset() noexcept;
    [[nodiscard]] bool submitCommand(const EngineCommand& command) noexcept;
    // Call only after the host audio callback has been stopped.
    void synchronizePendingCommands() noexcept;

    [[nodiscard]] StereoSample processSample() noexcept;
    void processBlock(float* leftOutput,
                      float* rightOutput,
                      std::size_t sampleCount,
                      const float* externalLeft = nullptr,
                      const float* externalRight = nullptr) noexcept;
    [[nodiscard]] bool popRecordedFrame(StereoSample& sample) noexcept;
    [[nodiscard]] std::uint64_t droppedRecordingFrames() const noexcept;
    [[nodiscard]] bool isRecordingActive() const noexcept;
    [[nodiscard]] StereoSample consumeOutputPeak() noexcept;
    [[nodiscard]] OutputSafetyTelemetry consumeOutputSafetyTelemetry() noexcept;
    [[nodiscard]] std::size_t outputLatencySamples() const noexcept;
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
    void applyOutputControlTargets() noexcept;
    void applyCommand(const EngineCommand& command) noexcept;
    void publishTransportTelemetry() noexcept;
    void publishOutputSafetyTelemetry(OutputStageTelemetry telemetry,
                                      float rmsLeft,
                                      float rmsRight) noexcept;
    [[nodiscard]] StereoSample renderSample() noexcept;
    [[nodiscard]] StereoSample finalizeOutput(StereoSample program,
                                              StereoSample external) noexcept;
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
    OutputStage outputStage;
    OutputProfile outputProfile = OutputProfile::legacy;
    RecordingFifo<65536> recordingFifo;
    bool recording = false;
    std::atomic<bool> recordingActive {false};
    std::atomic<float> outputPeakLeft {0.0F};
    std::atomic<float> outputPeakRight {0.0F};
    std::atomic<float> outputRmsLeft {0.0F};
    std::atomic<float> outputRmsRight {0.0F};
    std::atomic<float> outputInputSamplePeak {0.0F};
    std::atomic<float> outputInputTruePeak {0.0F};
    std::atomic<float> outputTruePeak {0.0F};
    std::atomic<float> outputGainReductionDb {0.0F};
    std::atomic<std::uint64_t> outputNonFiniteSamples {0};
    std::atomic<bool> outputCeilingEngaged {false};
    std::atomic<float> requestedOutputTrimDb {0.0F};
    std::atomic<bool> requestedOutputMuted {false};
    std::atomic<bool> requestedOutputSuspended {false};
    float appliedOutputTrimDb = 0.0F;
    bool appliedOutputMuted = false;
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
    std::array<std::atomic<double>, 2> telemetrySourcePlayhead {
        std::atomic<double> {-1.0}, std::atomic<double> {-1.0}};
    std::array<std::atomic<std::uint16_t>, stepsPerPattern> telemetryPatternRow {};
    // GUI gestures, macros and the detached PERFORM window can enqueue a
    // short burst of commands before the next audio callback drains them.
    // Keep enough headroom that valid bursts do not look like dead buttons.
    EngineCommandQueue<1024> commandQueue;
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
