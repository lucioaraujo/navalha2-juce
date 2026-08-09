#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>

#include "core/WavStreamWriter.h"

namespace navalha
{
struct WavMetadataRewriteReport
{
    std::uint64_t riffBytes = 0;
    std::uint64_t audioDataBytes = 0;
    std::size_t infoListsRemoved = 0;
    bool infoListWritten = false;
};

[[nodiscard]] WavMetadataRewriteReport rewriteWavInfoMetadata(
    std::istream& input, std::ostream& output, WavMetadata metadata);
}
