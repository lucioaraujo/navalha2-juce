#include "core/WavMemoryReader.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace navalha
{
namespace
{
std::uint16_t u16(std::span<const std::uint8_t> data, std::size_t offset)
{
    if (offset + 2 > data.size())
        throw std::invalid_argument("Truncated WAV");
    return static_cast<std::uint16_t>(data[offset])
        | static_cast<std::uint16_t>(data[offset + 1] << 8U);
}

std::uint32_t u32(std::span<const std::uint8_t> data, std::size_t offset)
{
    if (offset + 4 > data.size())
        throw std::invalid_argument("Truncated WAV");
    return static_cast<std::uint32_t>(data[offset])
        | (static_cast<std::uint32_t>(data[offset + 1]) << 8U)
        | (static_cast<std::uint32_t>(data[offset + 2]) << 16U)
        | (static_cast<std::uint32_t>(data[offset + 3]) << 24U);
}

bool tag(std::span<const std::uint8_t> data, std::size_t offset, const char* expected)
{
    return offset + 4 <= data.size()
        && std::memcmp(data.data() + offset, expected, 4) == 0;
}
}

std::unique_ptr<StereoAudioBuffer> decodeWav(std::span<const std::uint8_t> bytes,
                                             std::size_t frameLimit)
{
    if (bytes.size() < 44 || !tag(bytes, 0, "RIFF") || !tag(bytes, 8, "WAVE"))
        throw std::invalid_argument("Input is not a RIFF/WAVE file");

    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint16_t bits = 0;
    std::uint16_t blockAlign = 0;
    std::uint32_t sampleRate = 0;
    std::span<const std::uint8_t> audioData;

    std::size_t offset = 12;
    while (offset + 8 <= bytes.size())
    {
        const auto chunkSize = static_cast<std::size_t>(u32(bytes, offset + 4));
        const auto payload = offset + 8;
        if (payload + chunkSize > bytes.size())
            throw std::invalid_argument("WAV chunk exceeds input size");

        if (tag(bytes, offset, "fmt ") && chunkSize >= 16)
        {
            format = u16(bytes, payload);
            channels = u16(bytes, payload + 2);
            sampleRate = u32(bytes, payload + 4);
            blockAlign = u16(bytes, payload + 12);
            bits = u16(bytes, payload + 14);
            if (format == 0xfffe && chunkSize >= 40
                && u16(bytes, payload + 16) >= 22
                && u32(bytes, payload + 24) <= 3
                && u32(bytes, payload + 28) == 0x00100000
                && u32(bytes, payload + 32) == 0xaa000080
                && u32(bytes, payload + 36) == 0x719b3800)
                format = static_cast<std::uint16_t>(
                    u32(bytes, payload + 24));
        }
        else if (tag(bytes, offset, "data"))
        {
            audioData = bytes.subspan(payload, chunkSize);
        }
        offset = payload + chunkSize + (chunkSize & 1U);
    }

    const auto validFormat = (format == 1 && (bits == 16 || bits == 24))
        || (format == 3 && bits == 32);
    if (!validFormat || (channels != 1 && channels != 2)
        || sampleRate == 0 || blockAlign == 0 || audioData.empty())
        throw std::invalid_argument("Unsupported or incomplete WAV format");
    if (blockAlign != channels * bits / 8U || audioData.size() % blockAlign != 0)
        throw std::invalid_argument("Invalid WAV block alignment");

    const auto frames = audioData.size() / blockAlign;
    if (frames == 0 || frames > frameLimit)
        throw std::length_error("Decoded WAV exceeds the frame safety limit");

    std::vector<float> left(frames);
    std::vector<float> right(frames);
    const auto bytesPerSample = bits / 8U;
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const auto decode = [&] (std::size_t channel)
        {
            const auto position = frame * blockAlign + channel * bytesPerSample;
            if (format == 3)
                return std::bit_cast<float>(u32(audioData, position));
            if (bits == 16)
                return static_cast<float>(
                    static_cast<std::int16_t>(u16(audioData, position))) / 32768.0F;

            auto value = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(audioData[position])
                | (static_cast<std::uint32_t>(audioData[position + 1]) << 8U)
                | (static_cast<std::uint32_t>(audioData[position + 2]) << 16U));
            if ((value & 0x00800000) != 0)
                value -= 0x01000000;
            return static_cast<float>(value) / 8388608.0F;
        };
        left[frame] = decode(0);
        right[frame] = channels == 2 ? decode(1) : left[frame];
        if (!std::isfinite(left[frame]))
            left[frame] = 0.0F;
        if (!std::isfinite(right[frame]))
            right[frame] = 0.0F;
    }

    return std::make_unique<StereoAudioBuffer>(
        static_cast<double>(sampleRate), std::move(left), std::move(right));
}
}
