#include "core/FragmentGesture.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace navalha
{
namespace
{
void validateClock(double bpm, std::size_t divisionMode, double sampleRate)
{
    if (!std::isfinite(bpm) || bpm < 20.0 || bpm > 400.0
        || divisionMode > 3 || !std::isfinite(sampleRate)
        || sampleRate < 8000.0 || sampleRate > 384000.0)
        throw std::out_of_range("Invalid fragment gesture clock");
}

std::uint64_t spacingFrames(double bpm,
                            std::size_t divisionMode,
                            double sampleRate,
                            double divisor,
                            double minimumMs,
                            double maximumMs)
{
    const auto intervalMs =
        60000.0 / (bpm * static_cast<double>(divisionMode + 1));
    const auto spacingMs = std::clamp(
        intervalMs / divisor, minimumMs, maximumMs);
    return static_cast<std::uint64_t>(
        std::llround(spacingMs * sampleRate / 1000.0));
}

void fillOffsets(FragmentGesturePlan& plan, std::uint64_t spacing) noexcept
{
    for (std::size_t index = 0; index < plan.count; ++index)
        plan.frameOffsets[index] = spacing * index;
}
}

FragmentGesturePlan planStutter(std::uint16_t focusedCell,
                                double bpm,
                                std::size_t divisionMode,
                                double sampleRate)
{
    validateClock(bpm, divisionMode, sampleRate);
    static_cast<void>(PatternCell::decode(focusedCell));
    FragmentGesturePlan plan;
    plan.count = 4;
    std::fill_n(plan.cells.begin(), plan.count, focusedCell);
    fillOffsets(plan, spacingFrames(
        bpm, divisionMode, sampleRate, 4.0, 14.0, 160.0));
    return plan;
}

FragmentGesturePlan planBurst(const Pattern& pattern,
                              std::size_t activeSource,
                              std::array<std::size_t, 2> sourceSliceCounts,
                              double bpm,
                              std::size_t divisionMode,
                              double sampleRate,
                              AssistedRng& random)
{
    validateClock(bpm, divisionMode, sampleRate);
    if (activeSource > 1 || sourceSliceCounts[0] == 0
        || sourceSliceCounts[0] > 128 || sourceSliceCounts[1] == 0
        || sourceSliceCounts[1] > 128)
        throw std::out_of_range("Invalid BURST source or slice count");

    std::array<std::uint16_t, 8> pool {};
    std::size_t poolSize = 0;
    for (const auto code : pattern)
        if (PatternCell::decode(code).kind != PatternCell::Kind::gap)
            pool[poolSize++] = code;
    if (poolSize == 0)
    {
        const auto count = std::min<std::size_t>(
            8, sourceSliceCounts[activeSource]);
        for (std::size_t slice = 0; slice < count; ++slice)
            pool[poolSize++] = static_cast<std::uint16_t>(
                slice + (activeSource == 1 ? 128 : 0));
    }

    FragmentGesturePlan plan;
    plan.count = 8;
    std::size_t output = 0;
    while (output < plan.count)
    {
        auto pass = pool;
        for (std::size_t index = poolSize; index > 1; --index)
        {
            const auto selected = std::min<std::size_t>(
                index - 1,
                static_cast<std::size_t>(std::floor(random.next() * index)));
            std::swap(pass[index - 1], pass[selected]);
        }
        for (std::size_t index = 0; index < poolSize; ++index)
        {
            if (output == plan.count)
                break;
            plan.cells[output++] = pass[index];
        }
    }
    fillOffsets(plan, spacingFrames(
        bpm, divisionMode, sampleRate, 8.0, 18.0, 72.0));
    return plan;
}
}
