#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

#include "core/StereoMixer.h"

namespace navalha
{
enum class WavSampleFormat
{
    pcm16,
    pcm24,
    float32
};

enum class WavDitherMode
{
    none,
    tpdf
};

struct WavEncodingOptions
{
    WavDitherMode dither = WavDitherMode::tpdf;
    std::uint32_t ditherSeed = 0x4e41564cU;
};

struct WavMetadata
{
    std::string title;
    std::string artist;
    std::string project;
    std::string year;
    std::string comment;
};

void writeWavInfoListChunk(
    std::ostream& output, const WavMetadata& metadata);

class WavStreamWriter
{
public:
    WavStreamWriter(std::ostream& output,
                    std::uint32_t sampleRate,
                    WavSampleFormat format,
                    WavMetadata metadata = {},
                    WavEncodingOptions options = {});
    ~WavStreamWriter();

    WavStreamWriter(const WavStreamWriter&) = delete;
    WavStreamWriter& operator=(const WavStreamWriter&) = delete;

    void writeFrame(StereoSample sample);
    void finalize();

    [[nodiscard]] std::uint64_t framesWritten() const noexcept;
    [[nodiscard]] bool isFinalized() const noexcept;

private:
    void writeHeader();
    void writeInfoList();
    void writePcm16(float sample);
    void writePcm24(float sample);
    void writeFloat32(float sample);
    [[nodiscard]] double tpdfDitherLsb() noexcept;
    [[nodiscard]] std::uint32_t nextDitherRandom() noexcept;

    std::ostream& stream;
    std::uint32_t rate;
    WavSampleFormat sampleFormat;
    std::uint64_t frameCount = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint16_t formatCode = 0;
    WavMetadata tags;
    WavEncodingOptions encoding;
    std::uint32_t ditherState = 0;
    std::streamoff dataSizeOffset = 40;
    std::streamoff dataStartOffset = 44;
    bool finalized = false;
};
}
