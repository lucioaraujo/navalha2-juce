#include "core/LegacyFormat.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace navalha
{
namespace
{
constexpr std::size_t maximumLegacySlices = 4096;

std::string trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string withoutFudiSemicolon(std::string value)
{
    value = trim(std::move(value));
    if (!value.empty() && value.back() == ';')
        value.pop_back();
    return trim(std::move(value));
}

double number(std::string_view text, std::string_view context)
{
    std::size_t consumed = 0;
    double value = 0.0;
    try
    {
        value = std::stod(std::string(text), &consumed);
    }
    catch (const std::exception&)
    {
        throw std::invalid_argument("Invalid number in legacy "
                                    + std::string(context));
    }
    if (consumed != text.size() || !std::isfinite(value))
        throw std::invalid_argument("Invalid number in legacy "
                                    + std::string(context));
    return value;
}

std::string basename(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    const auto slash = value.find_last_of('/');
    return slash == std::string::npos ? value : value.substr(slash + 1);
}
}

LegacyNvl parseLegacyNvl(std::string_view text, std::string_view name)
{
    std::array<double, maximumLegacySlices> starts {};
    std::array<double, maximumLegacySlices> ends {};
    std::array<bool, maximumLegacySlices> hasStart {};
    std::array<bool, maximumLegacySlices> hasEnd {};
    LegacyNvl result;
    result.name = basename(std::string(name));
    std::size_t largestIndex = 0;
    bool hasSlices = false;

    std::istringstream lines {std::string(text)};
    for (std::string raw; std::getline(lines, raw);)
    {
        const auto line = withoutFudiSemicolon(std::move(raw));
        if (line.empty() || line.starts_with('#'))
            continue;
        std::istringstream fields(line);
        std::string key;
        fields >> key;
        std::transform(key.begin(), key.end(), key.begin(), [] (unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        if (key == "filename" || key == "pattern")
        {
            std::string value;
            std::getline(fields, value);
            if (key == "filename") result.sampleReference = trim(std::move(value));
            else result.patternReference = trim(std::move(value));
            continue;
        }
        if (key != "start" && key != "end")
        {
            result.unknownLines.push_back(line);
            continue;
        }

        std::string indexText;
        std::string valueText;
        fields >> indexText >> valueText;
        if (indexText.empty() || valueText.empty())
            throw std::invalid_argument("Incomplete slice line in " + result.name);
        const auto indexValue = number(indexText, result.name);
        if (std::floor(indexValue) != indexValue || indexValue < 0.0
            || indexValue >= static_cast<double>(maximumLegacySlices))
            throw std::invalid_argument("Legacy slice index out of range in " + result.name);
        const auto index = static_cast<std::size_t>(indexValue);
        const auto value = number(valueText, result.name);
        if (key == "start")
        {
            starts[index] = value;
            hasStart[index] = true;
        }
        else
        {
            ends[index] = value;
            hasEnd[index] = true;
        }
        largestIndex = std::max(largestIndex, index);
        hasSlices = true;
    }

    const auto count = hasSlices ? largestIndex + 1 : 0;
    result.storedSlices.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!hasStart[index] || !hasEnd[index])
            result.incompleteIndices.push_back(index);
        const Slice slice {starts[index], ends[index]};
        result.storedSlices.push_back(slice);
        if (slice.start > slice.end)
            result.invertedIndices.push_back(index);
        if (slice.start != 0.0 || slice.end != 0.0)
            result.operationalCount = index + 1;
    }
    return result;
}

LegacyPatterns parseLegacyPatterns(std::string_view text, std::string_view name)
{
    LegacyPatterns result;
    result.name = basename(std::string(name));
    std::vector<Pattern> imported;
    std::istringstream lines {std::string(text)};
    for (std::string raw; std::getline(lines, raw);)
    {
        const auto line = trim(std::move(raw));
        if (line.empty()) continue;
        if (line.starts_with("#matrix"))
        {
            std::istringstream header(line);
            std::string marker;
            std::string rows;
            std::string columns;
            header >> marker >> rows >> columns;
            if (rows.empty() || columns.empty())
                throw std::invalid_argument("Invalid #matrix header in " + result.name);
            const auto rowCount = number(rows, result.name);
            const auto columnCount = number(columns, result.name);
            if (std::floor(rowCount) != rowCount || std::floor(columnCount) != columnCount
                || rowCount < 0.0 || columnCount < 0.0)
                throw std::invalid_argument("Invalid #matrix dimensions in " + result.name);
            result.declaredRows = static_cast<std::size_t>(rowCount);
            result.declaredColumns = static_cast<std::size_t>(columnCount);
            continue;
        }
        if (line.starts_with('#')) continue;
        auto normalizedLine = line;
        std::replace(normalizedLine.begin(), normalizedLine.end(), ';', ' ');
        std::istringstream fields(normalizedLine);
        Pattern row {};
        std::size_t index = 0;
        for (std::string token; fields >> token;)
        {
            if (index >= stepsPerPattern) continue;
            const auto value = std::clamp(
                static_cast<long>(std::lround(number(token, result.name))),
                0L, 127L);
            row[index++] = static_cast<std::uint16_t>(value);
        }
        if (index != 0)
            imported.push_back(row);
    }
    result.sourceRows = imported.size();
    for (std::size_t index = 0; index < std::min(imported.size(), patternCount); ++index)
        result.rows[index] = imported[index];
    if (result.declaredRows != 0 && result.declaredRows != patternCount)
        result.warnings.push_back("Legacy matrix row count differs; imported first 10 rows");
    if (result.declaredColumns != 0 && result.declaredColumns != stepsPerPattern)
        result.warnings.push_back("Legacy matrix column count differs; imported first 8 columns");
    if (result.sourceRows < patternCount)
        result.warnings.push_back("Legacy pattern file has fewer than 10 rows; padded with zero");
    return result;
}

std::string encodeLegacyNvl(std::string_view sampleReference,
                            std::string_view patternReference,
                            const SliceBank& slices)
{
    std::ostringstream output;
    if (!sampleReference.empty()) output << "filename " << sampleReference << ";\n";
    if (!patternReference.empty()) output << "pattern " << patternReference << ";\n";
    output << std::setprecision(9);
    for (std::size_t index = 0; index < slices.size(); ++index)
    {
        const auto slice = slices.slices()[index];
        output << "start " << index << ' ' << slice.start << ";\n";
        output << "end " << index << ' ' << slice.end << ";\n";
    }
    return output.str();
}

std::string encodeLegacyPatterns(const PatternBank& patterns)
{
    std::ostringstream output;
    output << "#matrix " << patternCount << ' ' << stepsPerPattern << "\n";
    for (std::size_t row = 0; row < patternCount; ++row)
    {
        for (std::size_t step = 0; step < stepsPerPattern; ++step)
        {
            if (step != 0) output << ' ';
            output << patterns.cell(row, step);
        }
        output << "\n";
    }
    return output.str();
}
}
