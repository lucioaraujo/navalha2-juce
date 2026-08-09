#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/SessionModel.h"

namespace navalha
{
struct LegacyNvl
{
    std::string name;
    std::string sampleReference;
    std::string patternReference;
    std::vector<Slice> storedSlices;
    std::size_t operationalCount = 0;
    std::vector<std::size_t> incompleteIndices;
    std::vector<std::size_t> invertedIndices;
    std::vector<std::string> unknownLines;
};

struct LegacyPatterns
{
    std::string name;
    std::array<Pattern, patternCount> rows {};
    std::size_t sourceRows = 0;
    std::size_t declaredRows = 0;
    std::size_t declaredColumns = 0;
    std::vector<std::string> warnings;
};

[[nodiscard]] LegacyNvl parseLegacyNvl(
    std::string_view text, std::string_view name = "preset.nvl");
[[nodiscard]] LegacyPatterns parseLegacyPatterns(
    std::string_view text, std::string_view name = "patterns.ptn");

[[nodiscard]] std::string encodeLegacyNvl(
    std::string_view sampleReference,
    std::string_view patternReference,
    const SliceBank& slices);
[[nodiscard]] std::string encodeLegacyPatterns(const PatternBank& patterns);
}
