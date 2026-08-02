#include "core/PatternTransform.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace navalha
{
namespace
{
double hash01(double seed) noexcept
{
    const auto value = std::sin(seed * 12.9898 + 78.233) * 43758.5453123;
    return value - std::floor(value);
}

std::uint16_t encodeCell(std::size_t source, std::size_t slice) noexcept
{
    return static_cast<std::uint16_t>(
        std::min<std::size_t>(slice, 127) + (source == 1 ? 128 : 0));
}

std::size_t sourceFor(const PatternCell& cell, std::size_t step) noexcept
{
    if (cell.kind == PatternCell::Kind::sourceA)
        return 0;
    if (cell.kind == PatternCell::Kind::sourceB)
        return 1;
    return step % 2;
}

std::size_t deterministicSlice(std::size_t source,
                               double seed,
                               const std::array<std::size_t, 2>& counts) noexcept
{
    const auto count = std::max<std::size_t>(1, counts[source]);
    return std::min(count - 1,
                    static_cast<std::size_t>(std::floor(hash01(seed) * count)));
}
}

Pattern transformPattern(const Pattern& base,
                         const PatternMemory& memory,
                         std::array<std::size_t, 2> sourceSliceCounts,
                         std::size_t patternIndex,
                         PatternTransformAmounts amounts,
                         bool allowGaps)
{
    if (patternIndex >= patternCount)
        throw std::out_of_range("Pattern index must be between 0 and 9");
    if (sourceSliceCounts[0] == 0 || sourceSliceCounts[0] > 128
        || sourceSliceCounts[1] == 0 || sourceSliceCounts[1] > 128)
        throw std::out_of_range("Source slice counts must be between 1 and 128");
    const auto validAmount = [] (int value) { return value >= 0 && value <= 100; };
    if (!validAmount(amounts.mutation) || !validAmount(amounts.erosion)
        || !validAmount(amounts.deconstruct))
        throw std::out_of_range("Transform amounts must be between 0 and 100");

    const auto mutationIntensity = static_cast<double>(amounts.mutation) / 100.0;
    Pattern row = base;
    for (std::size_t step = 0; step < row.size(); ++step)
    {
        if (memory[step])
            continue;
        const auto gate = hash01(
            static_cast<double>((patternIndex + 1) * 101 + step * 17) + 0.37);
        if (gate > mutationIntensity)
            continue;
        const auto original = PatternCell::decode(base[step]);
        const auto operation = static_cast<int>(std::floor(hash01(
            static_cast<double>((patternIndex + 1) * 211 + step * 29) + 0.73) * 5.0));
        if (operation == 0)
        {
            if (allowGaps)
                row[step] = gapCellCode;
            else
            {
                const auto source = sourceFor(original, step);
                row[step] = encodeCell(source, deterministicSlice(
                    source, static_cast<double>(
                        (patternIndex + 1) * 181 + step * 31),
                    sourceSliceCounts));
            }
        }
        else if (operation == 1)
        {
            const auto sourceIndex = static_cast<std::size_t>(std::floor(
                hash01(static_cast<double>(
                    (patternIndex + 1) * 307 + step * 41)) * base.size()));
            row[step] = base[std::min(sourceIndex, base.size() - 1)];
        }
        else if (operation == 2 && original.kind != PatternCell::Kind::gap)
        {
            const auto target = original.kind == PatternCell::Kind::sourceA ? 1U : 0U;
            row[step] = encodeCell(
                target, original.sliceIndex % sourceSliceCounts[target]);
        }
        else if (operation == 3)
        {
            const auto source = sourceFor(original, step);
            row[step] = encodeCell(source, deterministicSlice(
                source, static_cast<double>(
                    (patternIndex + 1) * 419 + step * 53),
                sourceSliceCounts));
        }
        else
        {
            const auto displacement = 1 + static_cast<std::size_t>(std::floor(
                hash01(static_cast<double>(
                    (patternIndex + 1) * 503 + step * 67)) * 7.0));
            row[step] = base[(step + displacement) % base.size()];
        }
    }

    if (allowGaps)
    {
        const auto erosionIntensity = static_cast<double>(amounts.erosion) / 100.0;
        for (std::size_t step = 0; step < row.size(); ++step)
        {
            if (memory[step])
                continue;
            const auto threshold = hash01(
                static_cast<double>((patternIndex + 1) * 607 + step * 71) + 0.19);
            if (threshold < erosionIntensity)
                row[step] = gapCellCode;
        }
    }

    const auto deconstructInput = row;
    const auto deconstructIntensity =
        static_cast<double>(amounts.deconstruct) / 100.0;
    for (std::size_t step = 0; step < row.size(); ++step)
    {
        if (memory[step])
            continue;
        const auto threshold = hash01(
            static_cast<double>((patternIndex + 1) * 701 + step * 83) + 0.43);
        if (threshold > deconstructIntensity)
            continue;
        const auto operation = static_cast<int>(std::floor(hash01(
            static_cast<double>((patternIndex + 1) * 809 + step * 97) + 0.61) * 6.0));
        const auto decoded = PatternCell::decode(deconstructInput[step]);
        if (operation == 0)
        {
            if (allowGaps)
                row[step] = gapCellCode;
            else
            {
                const auto source = sourceFor(decoded, step);
                row[step] = encodeCell(source, deterministicSlice(
                    source, static_cast<double>(
                        823 + patternIndex * 89 + step * 17),
                    sourceSliceCounts));
            }
        }
        else if (operation == 1)
        {
            const auto displacement = static_cast<std::size_t>(std::floor(
                hash01(static_cast<double>(900 + patternIndex * 11 + step)) * 4.0));
            row[step] = base[(step + 7 - displacement) % base.size()];
        }
        else if (operation == 2 && decoded.kind != PatternCell::Kind::gap)
        {
            const auto target = decoded.kind == PatternCell::Kind::sourceA ? 1U : 0U;
            row[step] = encodeCell(
                target, decoded.sliceIndex % sourceSliceCounts[target]);
        }
        else if (operation == 3)
        {
            const auto source = sourceFor(decoded, step);
            row[step] = encodeCell(source, deterministicSlice(
                source, static_cast<double>(
                    1009 + patternIndex * 101 + step * 13),
                sourceSliceCounts));
        }
        else if (operation == 4)
            row[step] = deconstructInput[step == 0 ? 0 : step - 1];
        else
            row[step] = deconstructInput[(step * 3 + 5) % row.size()];
    }
    return row;
}
}
