#include "core/PortableArchive.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

#include "core/PortablePath.h"

namespace navalha
{
namespace
{
void put16(std::vector<std::uint8_t>& output, std::uint16_t value)
{
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void put32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
    for (unsigned int shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint16_t get16(std::span<const std::uint8_t> input, std::size_t offset)
{
    if (offset + 2 > input.size()) throw std::invalid_argument("Truncated ZIP");
    return static_cast<std::uint16_t>(input[offset])
        | static_cast<std::uint16_t>(input[offset + 1] << 8U);
}

std::uint32_t get32(std::span<const std::uint8_t> input, std::size_t offset)
{
    if (offset + 4 > input.size()) throw std::invalid_argument("Truncated ZIP");
    return static_cast<std::uint32_t>(input[offset])
        | (static_cast<std::uint32_t>(input[offset + 1]) << 8U)
        | (static_cast<std::uint32_t>(input[offset + 2]) << 16U)
        | (static_cast<std::uint32_t>(input[offset + 3]) << 24U);
}

std::uint32_t crc32(std::span<const std::uint8_t> data) noexcept
{
    auto crc = 0xffffffffU;
    for (const auto byte : data)
    {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1U) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

struct CentralRecord
{
    std::string path;
    std::uint32_t crc = 0;
    std::uint32_t size = 0;
    std::uint32_t offset = 0;
};
}

std::vector<std::uint8_t> encodePortableArchive(const std::vector<PortableEntry>& entries)
{
    if (entries.empty() || entries.size() > maxPortableEntries)
        throw std::length_error("Portable archive entry count is outside safety limits");

    std::vector<std::uint8_t> output;
    std::vector<CentralRecord> central;
    std::set<std::string, std::less<>> paths;
    std::size_t totalBytes = 0;
    for (const auto& entry : entries)
    {
        const auto path = normalizePortableRelativePath(entry.path);
        if (!paths.insert(path).second)
            throw std::invalid_argument("Duplicate portable archive path");
        if (path.size() > std::numeric_limits<std::uint16_t>::max()
            || entry.data.size() > std::numeric_limits<std::uint32_t>::max()
            || totalBytes + entry.data.size() > maxPortableBytes)
            throw std::length_error("Portable archive exceeds safety limits");
        totalBytes += entry.data.size();

        CentralRecord record {
            path, crc32(entry.data), static_cast<std::uint32_t>(entry.data.size()),
            static_cast<std::uint32_t>(output.size())
        };
        put32(output, 0x04034b50U);
        put16(output, 20); put16(output, 0); put16(output, 0);
        put16(output, 0); put16(output, 0);
        put32(output, record.crc); put32(output, record.size); put32(output, record.size);
        put16(output, static_cast<std::uint16_t>(path.size())); put16(output, 0);
        output.insert(output.end(), path.begin(), path.end());
        output.insert(output.end(), entry.data.begin(), entry.data.end());
        central.push_back(std::move(record));
    }

    const auto centralOffset = static_cast<std::uint32_t>(output.size());
    for (const auto& record : central)
    {
        put32(output, 0x02014b50U);
        put16(output, 20); put16(output, 20); put16(output, 0); put16(output, 0);
        put16(output, 0); put16(output, 0);
        put32(output, record.crc); put32(output, record.size); put32(output, record.size);
        put16(output, static_cast<std::uint16_t>(record.path.size()));
        put16(output, 0); put16(output, 0); put16(output, 0); put16(output, 0);
        put32(output, 0); put32(output, record.offset);
        output.insert(output.end(), record.path.begin(), record.path.end());
    }
    const auto centralSize = static_cast<std::uint32_t>(output.size()) - centralOffset;
    put32(output, 0x06054b50U);
    put16(output, 0); put16(output, 0);
    put16(output, static_cast<std::uint16_t>(central.size()));
    put16(output, static_cast<std::uint16_t>(central.size()));
    put32(output, centralSize); put32(output, centralOffset); put16(output, 0);
    return output;
}

std::vector<PortableEntry> decodePortableArchive(std::span<const std::uint8_t> archive,
                                                  std::size_t byteLimit)
{
    if (archive.size() > byteLimit)
        throw std::length_error("Portable archive exceeds input size limit");

    std::vector<PortableEntry> entries;
    std::set<std::string, std::less<>> paths;
    std::size_t offset = 0;
    std::size_t expandedBytes = 0;
    while (offset + 4 <= archive.size() && get32(archive, offset) == 0x04034b50U)
    {
        if (entries.size() == maxPortableEntries || offset + 30 > archive.size())
            throw std::length_error("Portable archive entry limit exceeded");
        const auto flags = get16(archive, offset + 6);
        const auto method = get16(archive, offset + 8);
        const auto expectedCrc = get32(archive, offset + 14);
        const auto compressedSize = get32(archive, offset + 18);
        const auto expandedSize = get32(archive, offset + 22);
        const auto nameSize = get16(archive, offset + 26);
        const auto extraSize = get16(archive, offset + 28);
        if (flags != 0 || method != 0 || compressedSize != expandedSize)
            throw std::invalid_argument("Only unencrypted stored ZIP entries are supported");
        const auto nameStart = offset + 30;
        const auto dataStart = nameStart + nameSize + extraSize;
        const auto dataEnd = dataStart + compressedSize;
        if (dataEnd > archive.size())
            throw std::invalid_argument("ZIP entry exceeds archive size");
        const std::string rawPath(
            reinterpret_cast<const char*>(archive.data() + nameStart), nameSize);
        const auto path = normalizePortableRelativePath(rawPath);
        if (!paths.insert(path).second)
            throw std::invalid_argument("Duplicate portable archive path");
        expandedBytes += expandedSize;
        if (expandedBytes > byteLimit)
            throw std::length_error("Portable archive expanded size limit exceeded");
        const auto data = archive.subspan(dataStart, expandedSize);
        if (crc32(data) != expectedCrc)
            throw std::invalid_argument("Portable archive CRC mismatch");
        entries.push_back({path, std::vector<std::uint8_t>(data.begin(), data.end())});
        offset = dataEnd;
    }
    if (entries.empty())
        throw std::invalid_argument("Portable archive contains no readable entries");
    return entries;
}
}
