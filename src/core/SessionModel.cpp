#include "core/SessionModel.h"

#include <algorithm>
#include <cmath>

namespace navalha
{
namespace
{
[[nodiscard]] std::size_t profileIndex(SliceBankProfile profile) noexcept
{
    return std::min<std::size_t>(
        static_cast<std::size_t>(profile), sliceBankProfileCount - 1);
}
}

NamedSliceBankStore::NamedSliceBankStore()
    : storage(std::make_unique<Storage>())
{
}

NamedSliceBankStore::NamedSliceBankStore(const NamedSliceBankStore& other)
    : storage(std::make_unique<Storage>(*other.storage))
{
}

NamedSliceBankStore& NamedSliceBankStore::operator=(
    const NamedSliceBankStore& other) noexcept
{
    if (this != &other)
        *storage = *other.storage;
    return *this;
}

SliceBankProfile NamedSliceBankStore::capture(
    const SliceBank& workingBank, SliceBankProfile profile) noexcept
{
    if (profile == SliceBankProfile::working)
        profile = SliceBankProfile::manual;
    set(profile, workingBank);
    storage->activeProfile = profile;
    return profile;
}

bool NamedSliceBankStore::apply(
    SliceBank& workingBank, SliceBankProfile profile) noexcept
{
    if (profile == SliceBankProfile::working)
    {
        storage->activeProfile = profile;
        return true;
    }
    const auto index = profileIndex(profile);
    if (!storage->valid[index])
    {
        auto generated = workingBank;
        const auto count = generatedSliceCount(profile);
        if (count != 0)
        {
            const auto slices = workingBank.slices();
            generated.divideRegion(
                slices.empty() ? 0.0 : slices.front().start,
                slices.empty() ? 1.0 : slices.back().end,
                count);
        }
        storage->banks[index] = std::move(generated);
        storage->valid[index] = true;
    }
    workingBank = storage->banks[index];
    storage->activeProfile = profile;
    return true;
}

void NamedSliceBankStore::set(
    SliceBankProfile profile, const SliceBank& bank) noexcept
{
    const auto index = profileIndex(profile);
    storage->banks[index] = bank;
    storage->valid[index] = profile != SliceBankProfile::working;
}

bool NamedSliceBankStore::has(SliceBankProfile profile) const noexcept
{
    return profile != SliceBankProfile::working
        && storage->valid[profileIndex(profile)];
}

const SliceBank& NamedSliceBankStore::bank(SliceBankProfile profile) const noexcept
{
    return storage->banks[profileIndex(profile)];
}

SliceBankProfile NamedSliceBankStore::active() const noexcept
{
    return storage->activeProfile;
}

void NamedSliceBankStore::setActive(SliceBankProfile profile) noexcept
{
    storage->activeProfile = static_cast<std::size_t>(profile) < sliceBankProfileCount
        ? profile : SliceBankProfile::working;
}

namespace
{
double clampUnit(double value) noexcept
{
    return std::clamp(value, 0.0, 1.0);
}
}

SessionModel::SessionModel()
    : sequencer(patterns)
{
}

bool Slice::isValid() const noexcept
{
    return std::isfinite(start) && std::isfinite(end)
        && start >= 0.0 && end <= 1.0 && end > start;
}

SliceBank::SliceBank()
{
    divideRegion(0.0, 1.0, 8);
}

void SliceBank::divideRegion(double start, double end, std::size_t count)
{
    start = clampUnit(start);
    end = clampUnit(end);

    if (end <= start)
        throw std::invalid_argument("Slice region must have positive duration");
    if (count == 0 || count > maxSlices)
        throw std::out_of_range("Slice count must be between 1 and 128");

    regionStart = start;
    regionEnd = end;
    valueCount = count;
    cutCount = count - 1;
    manualCutCount = 0;
    const auto width = (end - start) / static_cast<double>(count);

    for (std::size_t index = 0; index < count; ++index)
    {
        const auto sliceStart = start + width * static_cast<double>(index);
        const auto sliceEnd = index + 1 == count
            ? end
            : start + width * static_cast<double>(index + 1);
        values[index] = {sliceStart, sliceEnd};
        if (index + 1 < count)
            sliceCuts[index] = sliceEnd;
    }
}

void SliceBank::setSlice(std::size_t index, Slice slice)
{
    if (index >= valueCount)
        throw std::out_of_range("Slice index is outside the current bank");
    if (!slice.isValid())
        throw std::invalid_argument("Slice boundaries are invalid");

    values[index] = slice;
}

bool SliceBank::addBladeCut(double normalizedPosition) noexcept
{
    constexpr double minimumBoundaryDistance = 0.0005;
    constexpr double duplicateTolerance = 0.001;
    if (!std::isfinite(normalizedPosition)
        || normalizedPosition <= regionStart + minimumBoundaryDistance
        || normalizedPosition >= regionEnd - minimumBoundaryDistance
        || cutCount >= sliceCuts.size()
        || manualCutCount >= manualCutHistory.size())
        return false;

    for (std::size_t index = 0; index < cutCount; ++index)
        if (std::abs(sliceCuts[index] - normalizedPosition) < duplicateTolerance)
            return false;

    sliceCuts[cutCount++] = normalizedPosition;
    manualCutHistory[manualCutCount++] = normalizedPosition;
    rebuildBladeSlices();
    return true;
}

bool SliceBank::undoBladeCut() noexcept
{
    if (manualCutCount == 0)
        return false;

    const auto cut = manualCutHistory[--manualCutCount];
    for (std::size_t index = 0; index < cutCount; ++index)
    {
        if (std::abs(sliceCuts[index] - cut) <= 1.0e-9)
        {
            for (std::size_t move = index + 1; move < cutCount; ++move)
                sliceCuts[move - 1] = sliceCuts[move];
            --cutCount;
            break;
        }
    }
    rebuildBladeSlices();
    return true;
}

std::size_t SliceBank::appendMicroSlices(std::size_t sliceIndex,
                                         std::size_t requestedDivisions,
                                         double sourceDurationSeconds)
{
    if (sliceIndex >= valueCount)
        throw std::out_of_range("Micro-slice source index is outside the bank");
    if (requestedDivisions < 2 || requestedDivisions > 8)
        throw std::out_of_range("Micro-slice divisions must be between 2 and 8");
    if (!std::isfinite(sourceDurationSeconds) || sourceDurationSeconds <= 0.0)
        throw std::invalid_argument("Micro-slice source duration must be positive");

    const auto capacity = maxSlices - valueCount;
    if (capacity < 2)
        return 0;
    const auto parent = values[sliceIndex];
    const auto selectedSeconds =
        (parent.end - parent.start) * sourceDurationSeconds;
    const auto byDuration = static_cast<std::size_t>(
        std::floor(selectedSeconds / 0.002));
    const auto divisions = std::min({
        requestedDivisions,
        capacity,
        std::max<std::size_t>(2, byDuration),
        static_cast<std::size_t>(8)
    });
    if (divisions < 2)
        return 0;

    const auto width =
        (parent.end - parent.start) / static_cast<double>(divisions);
    for (std::size_t index = 0; index < divisions; ++index)
    {
        const auto start =
            parent.start + width * static_cast<double>(index);
        const auto end = index + 1 == divisions
            ? parent.end
            : parent.start + width * static_cast<double>(index + 1);
        values[valueCount++] = {start, end};
    }
    return divisions;
}

std::span<const Slice> SliceBank::slices() const noexcept
{
    return {values.data(), valueCount};
}

std::size_t SliceBank::size() const noexcept
{
    return valueCount;
}

std::size_t SliceBank::bladeCutCount() const noexcept
{
    return manualCutCount;
}

void SliceBank::rebuildBladeSlices() noexcept
{
    std::array<double, maxSlices + 1> boundaries {};
    boundaries[0] = regionStart;
    for (std::size_t index = 0; index < cutCount; ++index)
        boundaries[index + 1] = sliceCuts[index];
    boundaries[cutCount + 1] = regionEnd;

    std::sort(boundaries.begin() + 1, boundaries.begin() + 1 + cutCount);
    valueCount = cutCount + 1;
    for (std::size_t index = 0; index < valueCount; ++index)
        values[index] = {boundaries[index], boundaries[index + 1]};
}

void SourceMixer::normalize() noexcept
{
    const auto normalizeChannel = [] (MixerChannel& channel)
    {
        channel.level = std::clamp(channel.level, 0.0, 1.25);
        channel.pan = std::clamp(channel.pan, -1.0, 1.0);
        channel.width = std::clamp(channel.width, 0.0, 2.0);
    };

    normalizeChannel(sourceA);
    normalizeChannel(sourceB);
    balance = std::clamp(balance, -1.0, 1.0);
}

double SourceMixer::effectiveLevel(std::size_t sourceIndex) const
{
    if (sourceIndex > 1)
        throw std::out_of_range("Source index must be A/0 or B/1");

    const auto& channel = sourceIndex == 0 ? sourceA : sourceB;
    const auto soloActive = sourceA.solo || sourceB.solo;

    if (channel.muted || (soloActive && !channel.solo))
        return 0.0;

    const auto balanceFactor = sourceIndex == 0
        ? (balance > 0.0 ? 1.0 - balance : 1.0)
        : (balance < 0.0 ? 1.0 + balance : 1.0);

    return std::clamp(channel.level, 0.0, 1.25)
        * std::clamp(balanceFactor, 0.0, 1.0);
}

void SessionModel::selectSource(std::size_t sourceIndex)
{
    if (sourceIndex >= sources.size())
        throw std::out_of_range("Source index must be A/0 or B/1");

    activeSource = sourceIndex;
}

bool SessionModel::togglePatternMemory(std::size_t patternIndex,
                                       std::size_t stepIndex)
{
    if (patternIndex >= patternCount || stepIndex >= stepsPerPattern)
        throw std::out_of_range("MEMORY cell is outside the pattern bank");
    auto& protectedCell = patternMemory[patternIndex][stepIndex];
    protectedCell = !protectedCell;
    return protectedCell;
}

void SessionModel::applyPatternTransform(std::size_t patternIndex,
                                         PatternTransformAmounts amounts,
                                         bool allowGaps)
{
    if (patternIndex >= patternCount)
        throw std::out_of_range("Pattern index must be between 0 and 9");
    const auto validAmount = [] (int value)
    {
        return value >= 0 && value <= 100;
    };
    if (!validAmount(amounts.mutation) || !validAmount(amounts.erosion)
        || !validAmount(amounts.deconstruct))
        throw std::out_of_range("Transform amounts must be between 0 and 100");
    const auto total = static_cast<long long>(amounts.mutation)
        + amounts.erosion + amounts.deconstruct;
    if (total == 0)
    {
        if (patternTransform.hasBase
            && patternTransform.patternIndex == patternIndex)
            patterns.setPattern(patternIndex, patternTransform.base);
        patternTransform = {};
        return;
    }
    if (!patternTransform.hasBase
        || patternTransform.patternIndex != patternIndex)
    {
        patternTransform.hasBase = true;
        patternTransform.patternIndex = patternIndex;
        patternTransform.base = patterns.pattern(patternIndex);
    }
    patternTransform.amounts = amounts;
    patterns.setPattern(patternIndex, transformPattern(
        patternTransform.base, patternMemory[patternIndex],
        {sources[0].sliceBank.size(), sources[1].sliceBank.size()},
        patternIndex, amounts, allowGaps));
}

void SessionModel::commitPatternTransform() noexcept
{
    patternTransform = {};
}

void SessionModel::restorePatternTransform()
{
    if (patternTransform.hasBase)
        patterns.setPattern(
            patternTransform.patternIndex, patternTransform.base);
    patternTransform = {};
}

void SessionModel::applyCurrentFormSceneMaterial()
{
    const auto& form = formDirector.state();
    const auto& scene = form.scenes[form.currentScene];
    const std::array profiles {scene.bankA, scene.bankB};
    for (std::size_t source = 0; source < sources.size(); ++source)
    {
        static_cast<void>(formSliceBanks[source].apply(
            sources[source].sliceBank, profiles[source]));
    }
}

SliceBankProfile SessionModel::captureFormSliceBank(
    std::size_t sourceIndex, SliceBankProfile profile) noexcept
{
    if (sourceIndex >= sources.size())
        return SliceBankProfile::working;
    return formSliceBanks[sourceIndex].capture(
        sources[sourceIndex].sliceBank, profile);
}

AssistedPerformanceContext SessionModel::performanceContext(
    double manualVariation) const noexcept
{
    return assistedPerformanceContext(
        formDirector.state(), manualVariation);
}
}
