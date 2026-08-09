#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/WavStreamWriter.h"

namespace navalha
{
constexpr std::size_t maximumTakeCatalogEntries = 512;

struct TakeReview
{
    std::string status = "EXPERIMENT";
    int rating = 0;
    std::string tags;
    std::string notes;
};

struct TakeEntry
{
    std::string id;
    std::string audioPath;
    std::string filename;
    std::string createdAt;
    double durationSeconds = 0.0;
    std::uint64_t frames = 0;
    std::uint32_t sampleRate = 0;
    WavSampleFormat sampleFormat = WavSampleFormat::pcm24;
    WavMetadata metadata;
    TakeReview review;
    std::string recipeJson;
};

class TakeCatalog
{
public:
    [[nodiscard]] const std::vector<TakeEntry>& entries() const noexcept;
    [[nodiscard]] TakeEntry* find(std::string_view id) noexcept;
    [[nodiscard]] const TakeEntry* find(std::string_view id) const noexcept;
    [[nodiscard]] const TakeEntry* findByAudioPath(
        std::string_view audioPath) const noexcept;
    void upsert(TakeEntry entry);
    bool remove(std::string_view id) noexcept;
    void clear() noexcept;

private:
    std::vector<TakeEntry> values;
};

void normalizeTakeEntry(TakeEntry& entry);
void normalizeWavMetadata(WavMetadata& metadata);
[[nodiscard]] std::string encodeTakeCatalog(const TakeCatalog& catalog);
[[nodiscard]] TakeCatalog decodeTakeCatalog(std::string_view json);
[[nodiscard]] const char* toString(WavSampleFormat format) noexcept;
[[nodiscard]] WavSampleFormat wavSampleFormatFromString(
    std::string_view value, WavSampleFormat fallback = WavSampleFormat::pcm24) noexcept;
}
