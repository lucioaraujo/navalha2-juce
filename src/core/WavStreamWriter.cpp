#include "core/WavStreamWriter.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace navalha
{
namespace
{
void writeU16(std::ostream& stream, std::uint16_t value)
{
    stream.put(static_cast<char>(value & 0xffU));
    stream.put(static_cast<char>((value >> 8U) & 0xffU));
}

void writeU32(std::ostream& stream, std::uint32_t value)
{
    for (unsigned int shift = 0; shift < 32; shift += 8)
        stream.put(static_cast<char>((value >> shift) & 0xffU));
}
}

WavStreamWriter::WavStreamWriter(std::ostream& output,
                                 std::uint32_t sampleRate,
                                 WavSampleFormat format,
                                 WavMetadata metadata)
    : stream(output), rate(sampleRate), sampleFormat(format), tags(std::move(metadata))
{
    if (rate == 0)
        throw std::invalid_argument("WAV sample rate must be positive");
    bitsPerSample = sampleFormat == WavSampleFormat::pcm16 ? 16
        : sampleFormat == WavSampleFormat::pcm24 ? 24 : 32;
    formatCode = sampleFormat == WavSampleFormat::float32 ? 3 : 1;
    writeHeader();
}

WavStreamWriter::~WavStreamWriter()
{
    try
    {
        finalize();
    }
    catch (...)
    {
    }
}

void WavStreamWriter::writeFrame(StereoSample sample)
{
    if (finalized)
        throw std::logic_error("Cannot write to a finalized WAV stream");
    const auto bytesPerFrame = static_cast<std::uint64_t>(bitsPerSample / 8U) * 2U;
    if (frameCount >= std::numeric_limits<std::uint32_t>::max() / bytesPerFrame)
        throw std::length_error("WAV reached the RIFF 4 GiB frame limit");

    const auto writeSample = [this] (float value)
    {
        if (sampleFormat == WavSampleFormat::pcm16)
            writePcm16(value);
        else if (sampleFormat == WavSampleFormat::pcm24)
            writePcm24(value);
        else
            writeFloat32(value);
    };
    writeSample(sample.left);
    writeSample(sample.right);
    ++frameCount;
}

void WavStreamWriter::finalize()
{
    if (finalized)
        return;

    const auto bytesPerFrame = static_cast<std::uint64_t>(bitsPerSample / 8U) * 2U;
    const auto dataBytes64 = frameCount * bytesPerFrame;
    const auto endPosition = stream.tellp();
    if (endPosition < 0 || dataBytes64 > std::numeric_limits<std::uint32_t>::max()
        || static_cast<std::uint64_t>(endPosition) > std::numeric_limits<std::uint32_t>::max() + 8ULL)
        throw std::length_error("WAV exceeds the RIFF 4 GiB size limit");

    const auto dataBytes = static_cast<std::uint32_t>(dataBytes64);
    stream.seekp(4);
    writeU32(stream, static_cast<std::uint32_t>(endPosition) - 8U);
    stream.seekp(dataSizeOffset);
    writeU32(stream, dataBytes);
    stream.seekp(0, std::ios::end);
    stream.flush();
    if (!stream)
        throw std::runtime_error("Failed to finalize WAV stream");
    finalized = true;
}

std::uint64_t WavStreamWriter::framesWritten() const noexcept { return frameCount; }
bool WavStreamWriter::isFinalized() const noexcept { return finalized; }

void WavStreamWriter::writeHeader()
{
    stream.write("RIFF", 4);
    writeU32(stream, 0);
    stream.write("WAVEfmt ", 8);
    writeU32(stream, 16);
    writeU16(stream, formatCode);
    writeU16(stream, 2);
    writeU32(stream, rate);
    const auto blockAlign = static_cast<std::uint16_t>(2U * bitsPerSample / 8U);
    writeU32(stream, rate * blockAlign);
    writeU16(stream, blockAlign);
    writeU16(stream, bitsPerSample);
    writeInfoList();
    stream.write("data", 4);
    dataSizeOffset = stream.tellp();
    writeU32(stream, 0);
    dataStartOffset = stream.tellp();
    if (!stream)
        throw std::runtime_error("Failed to write WAV header");
}

void WavStreamWriter::writeInfoList()
{
    const auto hasMetadata = !tags.title.empty() || !tags.artist.empty()
        || !tags.project.empty() || !tags.year.empty() || !tags.comment.empty();
    if (!hasMetadata)
        return;

    const auto listStart = stream.tellp();
    stream.write("LIST", 4);
    writeU32(stream, 0);
    stream.write("INFO", 4);

    const auto writeTag = [this] (const char* id, const std::string& input)
    {
        if (input.empty())
            return;
        auto value = input.substr(0, 4095);
        for (auto& character : value)
            if (character == '\0')
                character = ' ';
        stream.write(id, 4);
        const auto size = static_cast<std::uint32_t>(value.size() + 1);
        writeU32(stream, size);
        stream.write(value.data(), static_cast<std::streamsize>(value.size()));
        stream.put('\0');
        if ((size & 1U) != 0)
            stream.put('\0');
    };

    writeTag("INAM", tags.title);
    writeTag("IART", tags.artist);
    writeTag("IPRD", tags.project);
    writeTag("ICRD", tags.year);
    writeTag("ICMT", tags.comment);

    const auto listEnd = stream.tellp();
    stream.seekp(listStart + std::streamoff(4));
    writeU32(stream, static_cast<std::uint32_t>(listEnd - listStart - std::streamoff(8)));
    stream.seekp(listEnd);
}

void WavStreamWriter::writePcm16(float sample)
{
    const auto value = static_cast<std::int16_t>(
        std::lround(std::clamp(sample, -1.0F, 1.0F) * 32767.0F));
    writeU16(stream, static_cast<std::uint16_t>(value));
}

void WavStreamWriter::writePcm24(float sample)
{
    const auto value = static_cast<std::int32_t>(
        std::lround(std::clamp(sample, -1.0F, 1.0F) * 8388607.0F));
    const auto bits = static_cast<std::uint32_t>(value);
    stream.put(static_cast<char>(bits & 0xffU));
    stream.put(static_cast<char>((bits >> 8U) & 0xffU));
    stream.put(static_cast<char>((bits >> 16U) & 0xffU));
}

void WavStreamWriter::writeFloat32(float sample)
{
    writeU32(stream, std::bit_cast<std::uint32_t>(sample));
}
}
