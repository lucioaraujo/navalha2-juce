#pragma once

#include <array>
#include <cstddef>

#include "core/Sequencer.h"

namespace navalha
{
using PatternMemory = std::array<bool, stepsPerPattern>;

struct PatternTransformAmounts
{
    int mutation = 0;
    int erosion = 0;
    int deconstruct = 0;
};

struct PatternTransformState
{
    bool hasBase = false;
    std::size_t patternIndex = 0;
    Pattern base {};
    PatternTransformAmounts amounts;
};

[[nodiscard]] Pattern transformPattern(
    const Pattern& base,
    const PatternMemory& memory,
    std::array<std::size_t, 2> sourceSliceCounts,
    std::size_t patternIndex,
    PatternTransformAmounts amounts,
    bool allowGaps = true);
}
