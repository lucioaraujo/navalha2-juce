#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace navalha
{
enum class EngineCommandType
{
    start,
    stop,
    reset,
    setTempo,
    selectPattern,
    selectSource,
    setPatternCell,
    togglePatternMemory,
    applyPatternTransform,
    commitPatternTransform,
    restorePatternTransform,
    setMixerChannel,
    setMixerBalance,
    divideSliceRegion,
    setSlice,
    addBladeCut,
    undoBladeCut,
    appendMicroSlices,
    setTiming,
    setMasterLevel,
    setHeritagePitch,
    setVirtualVoiceProperty,
    setVirtualVoicePatternCell,
    triggerSlice,
    startStutter,
    startBurst,
    startTraceLoop,
    stopTraceLoop,
    clearControlTrace,
    appendControlTracePoint,
    setFormEnabled,
    selectFormScene,
    toggleFormHold,
    advanceFormScene,
    resetFormDirector,
    setFormSceneBasic,
    setFormSceneProfiles,
    setFormSceneCharacter,
    checkpointFormEdit,
    toggleFormSceneLock,
    addFormScene,
    duplicateFormScene,
    deleteFormScene,
    moveFormScene,
    undoFormEdit,
    redoFormEdit,
    captureFormSliceBank,
    setAssistedSettings,
    setAssistedSeed,
    forceAssistedDecision,
    keepAssistedCuts,
    restoreAssistedCuts,
    setMotifLocks,
    startRecording,
    stopRecording
};

enum class VirtualVoiceProperty
{
    enabled,
    source,
    division,
    patternLength,
    focusStart,
    focusEnd,
    pitch,
    level,
    pan,
    attack,
    release
};

struct EngineCommand
{
    EngineCommandType type = EngineCommandType::stop;
    std::size_t indexA = 0;
    std::size_t indexB = 0;
    double valueA = 0.0;
    double valueB = 0.0;
    double valueC = 0.0;
};

template <std::size_t Capacity>
class EngineCommandQueue
{
    static_assert(Capacity >= 2);

public:
    [[nodiscard]] bool push(const EngineCommand& command) noexcept
    {
        const auto write = writeIndex.load(std::memory_order_relaxed);
        const auto next = increment(write);
        if (next == readIndex.load(std::memory_order_acquire))
            return false;

        commands[write] = command;
        writeIndex.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool pop(EngineCommand& command) noexcept
    {
        const auto read = readIndex.load(std::memory_order_relaxed);
        if (read == writeIndex.load(std::memory_order_acquire))
            return false;

        command = commands[read];
        readIndex.store(increment(read), std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] static constexpr std::size_t increment(std::size_t index) noexcept
    {
        return (index + 1) % Capacity;
    }

    std::array<EngineCommand, Capacity> commands {};
    alignas(64) std::atomic<std::size_t> writeIndex {0};
    alignas(64) std::atomic<std::size_t> readIndex {0};
};
}
