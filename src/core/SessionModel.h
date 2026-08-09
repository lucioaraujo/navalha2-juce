#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>

#include "core/Sequencer.h"
#include "core/AssistedRng.h"
#include "core/AssistedPerformer.h"
#include "core/PatternTransform.h"
#include "core/FormDirector.h"
#include "core/ControlTrace.h"

namespace navalha
{
constexpr std::size_t maxSlices = 128;

struct Slice
{
    double start = 0.0;
    double end = 1.0;

    [[nodiscard]] bool isValid() const noexcept;
};

class SliceBank
{
public:
    SliceBank();

    void divideRegion(double start, double end, std::size_t count);
    void setSlice(std::size_t index, Slice slice);
    [[nodiscard]] bool addBladeCut(double normalizedPosition) noexcept;
    [[nodiscard]] bool undoBladeCut() noexcept;
    [[nodiscard]] std::size_t appendMicroSlices(
        std::size_t sliceIndex,
        std::size_t requestedDivisions,
        double sourceDurationSeconds);

    [[nodiscard]] std::span<const Slice> slices() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t bladeCutCount() const noexcept;

private:
    void rebuildBladeSlices() noexcept;

    std::array<Slice, maxSlices> values {};
    std::array<double, maxSlices - 1> sliceCuts {};
    std::array<double, maxSlices - 1> manualCutHistory {};
    std::size_t valueCount = 0;
    std::size_t cutCount = 0;
    std::size_t manualCutCount = 0;
    double regionStart = 0.0;
    double regionEnd = 1.0;
};

constexpr std::size_t sliceBankProfileCount = 7;

class NamedSliceBankStore
{
public:
    NamedSliceBankStore();
    NamedSliceBankStore(const NamedSliceBankStore& other);
    NamedSliceBankStore& operator=(const NamedSliceBankStore& other) noexcept;
    NamedSliceBankStore(NamedSliceBankStore&&) noexcept = default;
    NamedSliceBankStore& operator=(NamedSliceBankStore&&) noexcept = default;

    [[nodiscard]] SliceBankProfile capture(
        const SliceBank& workingBank, SliceBankProfile profile) noexcept;
    [[nodiscard]] bool apply(
        SliceBank& workingBank, SliceBankProfile profile) noexcept;
    void set(SliceBankProfile profile, const SliceBank& bank) noexcept;
    [[nodiscard]] bool has(SliceBankProfile profile) const noexcept;
    [[nodiscard]] const SliceBank& bank(SliceBankProfile profile) const noexcept;
    [[nodiscard]] SliceBankProfile active() const noexcept;
    void setActive(SliceBankProfile profile) noexcept;

private:
    struct Storage
    {
        std::array<SliceBank, sliceBankProfileCount> banks {};
        std::array<bool, sliceBankProfileCount> valid {};
        SliceBankProfile activeProfile = SliceBankProfile::working;
    };

    std::unique_ptr<Storage> storage;
};

struct MixerChannel
{
    double level = 1.0;
    double pan = 0.0;
    double width = 1.0;
    bool muted = false;
    bool solo = false;
};

class SourceMixer
{
public:
    MixerChannel sourceA;
    MixerChannel sourceB;
    double balance = 0.0;

    [[nodiscard]] double effectiveLevel(std::size_t sourceIndex) const;
    void normalize() noexcept;
};

struct SourceState
{
    SliceBank sliceBank;
    bool hasAudio = false;
};

struct MotifLocks
{
    bool source = false;
    bool cuts = false;
    bool pattern = false;
    bool transform = false;
    bool pitch = false;
    bool gap = false;
    bool mix = false;
    bool voices = false;
};

struct VirtualVoiceState
{
    bool enabled = false;
    std::size_t sourceIndex = 0;
    std::size_t division = 2;
    std::array<std::uint8_t, 16> pattern {0, 1, 2, 3, 4, 5, 6, 7};
    std::size_t patternLength = 8;
    double focusStart = 0.0;
    double focusEnd = 1.0;
    int pitchSemitones = 0;
    double level = 0.28;
    double pan = 0.0;
    double attackSeconds = 0.008;
    double releaseSeconds = 0.080;
};

class SessionModel
{
public:
    SessionModel();

    std::array<SourceState, 2> sources;
    SourceMixer mixer;
    PatternBank patterns;
    Sequencer sequencer;
    std::array<VirtualVoiceState, 2> virtualVoices;
    std::array<PatternMemory, patternCount> patternMemory {};
    PatternTransformState patternTransform;
    FormDirector formDirector;
    std::array<NamedSliceBankStore, 2> formSliceBanks;
    ControlTrace controlTrace;
    AssistedRng assistedRng;
    AssistedPerformerSettings assisted;
    MotifLocks motifLocks;
    std::size_t activeSource = 0;
    double masterLevel = 0.8;
    int heritagePitchSemitones = 0;
    double heritagePitchMode = 0.0;

    void selectSource(std::size_t sourceIndex);
    [[nodiscard]] bool togglePatternMemory(
        std::size_t patternIndex, std::size_t stepIndex);
    void applyPatternTransform(std::size_t patternIndex,
                               PatternTransformAmounts amounts,
                               bool allowGaps = true);
    void commitPatternTransform() noexcept;
    void restorePatternTransform();
    void applyCurrentFormSceneMaterial();
    [[nodiscard]] SliceBankProfile captureFormSliceBank(
        std::size_t sourceIndex, SliceBankProfile profile) noexcept;
    [[nodiscard]] AssistedPerformanceContext performanceContext(
        double manualVariation) const noexcept;
};
}
