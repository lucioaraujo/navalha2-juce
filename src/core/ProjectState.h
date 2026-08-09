#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "core/SessionModel.h"

namespace navalha
{
struct SourceReference
{
    std::string filename;
    std::string relativePath;
    std::uint64_t size = 0;
    std::uint64_t lastModified = 0;
    std::string mediaType;
};

constexpr std::size_t motifSlotCount = 8;

struct MotifSnapshot
{
    bool occupied = false;
    std::string name;
    std::string capturedAt;
    std::array<SourceState, 2> sources;
    Pattern pattern {};
    PatternMemory cellMemory {};
    std::array<VirtualVoiceState, 2> virtualVoices;
    SourceMixer mixer;
    std::size_t activeSource = 0;
    std::size_t currentPattern = 0;
    double bpm = 120.0;
    std::size_t divisionMode = 0;
    TimingMode timingMode = TimingMode::grid;
    double jitter = 18.0;
    int heritagePitchSemitones = 0;
    double heritagePitchMode = 0.0;
};

struct ProjectStateV2
{
    int version = 2;
    std::array<SourceState, 2> sources;
    std::array<SourceReference, 2> sourceReferences;
    PatternBank patterns;
    SourceMixer mixer;
    std::array<VirtualVoiceState, 2> virtualVoices;
    std::array<PatternMemory, patternCount> patternMemory {};
    PatternTransformState patternTransform;
    FormDirectorState formDirector = defaultFormDirector();
    std::array<NamedSliceBankStore, 2> formSliceBanks;
    ControlTrace controlTrace;
    std::size_t activeSource = 0;
    std::size_t currentPattern = 0;
    double bpm = 120.0;
    std::size_t divisionMode = 0;
    TimingMode timingMode = TimingMode::grid;
    double jitter = 18.0;
    std::uint32_t timingSeed = 0x4e415632U;
    int heritagePitchSemitones = 0;
    double heritagePitchMode = 0.0;
    double masterLevel = 0.8;
    std::uint32_t assistedSeed = AssistedRng::defaultSeed;
    std::uint32_t assistedState = AssistedRng::defaultSeed;
    std::uint64_t assistedCursor = 0;
    AssistedPerformerSettings assisted;
    MotifLocks motifLocks;
    std::array<MotifSnapshot, motifSlotCount> motifSlots;
    std::size_t selectedMotifSlot = 0;
};

struct ProjectStateV1
{
    SourceState sourceA;
    SourceState sourceB;
    bool hasDualMaterialExtension = false;
    PatternBank patterns;
    std::size_t activeSource = 0;
    std::size_t currentPattern = 0;
    double bpm = 120.0;
    std::size_t divisionMode = 0;
    int pitchSemitones = 0;
    double pitchMode = 0.0;
    double masterLevel = 0.8;
};

[[nodiscard]] ProjectStateV2 captureProjectState(const SessionModel& session) noexcept;
[[nodiscard]] ProjectStateV2 migrateProjectV1(const ProjectStateV1& legacy) noexcept;
void restoreProjectState(const ProjectStateV2& project, SessionModel& session);
}
