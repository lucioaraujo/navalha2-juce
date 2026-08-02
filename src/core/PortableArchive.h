#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace navalha
{
constexpr std::size_t maxPortableEntries = 256;
constexpr std::size_t maxPortableBytes = 256 * 1024 * 1024;

struct PortableEntry
{
    std::string path;
    std::vector<std::uint8_t> data;
};

[[nodiscard]] std::vector<std::uint8_t> encodePortableArchive(
    const std::vector<PortableEntry>& entries);
[[nodiscard]] std::vector<PortableEntry> decodePortableArchive(
    std::span<const std::uint8_t> archive,
    std::size_t byteLimit = maxPortableBytes);
}
