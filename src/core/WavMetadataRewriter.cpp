#include "core/WavMetadataRewriter.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace navalha
{
namespace
{
void readExact(std::istream& input, char* destination, std::size_t bytes)
{
    input.read(destination, static_cast<std::streamsize>(bytes));
    if (input.gcount() != static_cast<std::streamsize>(bytes))
        throw std::invalid_argument("Truncated RIFF/WAVE input");
}

std::uint32_t readU32(std::istream& input)
{
    std::array<unsigned char, 4> bytes {};
    readExact(input, reinterpret_cast<char*>(bytes.data()), bytes.size());
    return static_cast<std::uint32_t>(bytes[0])
        | (static_cast<std::uint32_t>(bytes[1]) << 8U)
        | (static_cast<std::uint32_t>(bytes[2]) << 16U)
        | (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

void writeU32(std::ostream& output, std::uint32_t value)
{
    for (unsigned int shift = 0; shift < 32; shift += 8)
        output.put(static_cast<char>((value >> shift) & 0xffU));
}

void copyExact(std::istream& input,
               std::ostream& output,
               std::uint64_t bytes)
{
    std::array<char, 64 * 1024> buffer {};
    while (bytes != 0)
    {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(bytes, buffer.size()));
        readExact(input, buffer.data(), count);
        output.write(buffer.data(), static_cast<std::streamsize>(count));
        if (!output)
            throw std::runtime_error("Unable to write rewritten WAV");
        bytes -= count;
    }
}

void discardExact(std::istream& input, std::uint64_t bytes)
{
    std::array<char, 64 * 1024> buffer {};
    while (bytes != 0)
    {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(bytes, buffer.size()));
        readExact(input, buffer.data(), count);
        bytes -= count;
    }
}

void copyTrailingBytes(std::istream& input, std::ostream& output)
{
    std::array<char, 64 * 1024> buffer {};
    while (input)
    {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0)
            output.write(buffer.data(), count);
    }
    if (input.bad())
        throw std::runtime_error("Unable to read trailing WAV data");
    if (!output)
        throw std::runtime_error("Unable to preserve trailing WAV data");
}

bool sameTag(const char* value, const char* expected) noexcept
{
    return std::memcmp(value, expected, 4) == 0;
}
}

WavMetadataRewriteReport rewriteWavInfoMetadata(
    std::istream& input, std::ostream& output, WavMetadata metadata)
{
    std::array<char, 4> tag {};
    readExact(input, tag.data(), tag.size());
    if (!sameTag(tag.data(), "RIFF"))
        throw std::invalid_argument("Input is not a RIFF file");
    const auto originalRiffSize = readU32(input);
    readExact(input, tag.data(), tag.size());
    if (!sameTag(tag.data(), "WAVE") || originalRiffSize < 4U)
        throw std::invalid_argument("Input is not a RIFF/WAVE file");

    output.write("RIFF", 4);
    writeU32(output, 0);
    output.write("WAVE", 4);
    if (!output)
        throw std::runtime_error("Unable to start rewritten WAV");

    WavMetadataRewriteReport report;
    std::uint64_t remaining = originalRiffSize - 4U;
    bool hasFormat = false;
    bool hasAudioData = false;
    while (remaining != 0)
    {
        if (remaining < 8U)
            throw std::invalid_argument("Malformed RIFF chunk table");
        std::array<char, 4> id {};
        readExact(input, id.data(), id.size());
        const auto chunkSize = readU32(input);
        const auto paddedSize = static_cast<std::uint64_t>(chunkSize)
            + (chunkSize & 1U);
        if (paddedSize > remaining - 8U)
            throw std::invalid_argument("RIFF chunk exceeds declared size");
        remaining -= 8U + paddedSize;

        const auto isList = sameTag(id.data(), "LIST");
        std::array<char, 4> listType {};
        if (isList && chunkSize >= 4U)
            readExact(input, listType.data(), listType.size());
        const auto isInfo = isList && chunkSize >= 4U
            && sameTag(listType.data(), "INFO");
        if (isInfo)
        {
            discardExact(input, paddedSize - 4U);
            ++report.infoListsRemoved;
            continue;
        }

        output.write(id.data(), static_cast<std::streamsize>(id.size()));
        writeU32(output, chunkSize);
        if (isList && chunkSize >= 4U)
        {
            output.write(
                listType.data(), static_cast<std::streamsize>(listType.size()));
            copyExact(input, output, paddedSize - 4U);
        }
        else
        {
            copyExact(input, output, paddedSize);
        }
        hasFormat = hasFormat || sameTag(id.data(), "fmt ");
        if (sameTag(id.data(), "data"))
        {
            hasAudioData = true;
            report.audioDataBytes += chunkSize;
        }
    }

    if (!hasFormat || !hasAudioData)
        throw std::invalid_argument("RIFF/WAVE requires fmt and data chunks");

    const auto infoStart = output.tellp();
    writeWavInfoListChunk(output, metadata);
    const auto riffEnd = output.tellp();
    if (infoStart < 0 || riffEnd < 0 || riffEnd < infoStart)
        throw std::runtime_error("Unable to measure rewritten WAV");
    report.infoListWritten = riffEnd != infoStart;
    const auto riffBytes = static_cast<std::uint64_t>(riffEnd);
    if (riffBytes < 8U
        || riffBytes - 8U > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Rewritten WAV exceeds the RIFF 4 GiB limit");
    report.riffBytes = riffBytes;

    output.seekp(4);
    writeU32(output, static_cast<std::uint32_t>(riffBytes - 8U));
    output.seekp(riffEnd);
    copyTrailingBytes(input, output);
    output.flush();
    if (!output)
        throw std::runtime_error("Unable to finalize rewritten WAV");
    return report;
}
}
