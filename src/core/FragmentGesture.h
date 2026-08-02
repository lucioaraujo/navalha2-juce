#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/AssistedRng.h"
#include "core/Sequencer.h"

namespace navalha
{
struct FragmentGesturePlan
{
    std::array<std::uint16_t, 8> cells {};
    std::array<std::uint64_t, 8> frameOffsets {};
    std::size_t count = 0;
};

[[nodiscard]] FragmentGesturePlan planStutter(
    std::uint16_t focusedCell,
    double bpm,
    std::size_t divisionMode,
    double sampleRate);

[[nodiscard]] FragmentGesturePlan planBurst(
    const Pattern& pattern,
    std::size_t activeSource,
    std::array<std::size_t, 2> sourceSliceCounts,
    double bpm,
    std::size_t divisionMode,
    double sampleRate,
    AssistedRng& random);
}
