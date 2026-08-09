#include "core/TakeCatalog.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "core/Json.h"

#ifndef NAVALHA_JUCE_VERSION
#define NAVALHA_JUCE_VERSION "0.1.0"
#endif

namespace navalha
{
namespace
{
constexpr std::size_t maximumCatalogBytes = 8 * 1024 * 1024;
constexpr std::size_t maximumRecipeBytes = 256 * 1024;

const Json* child(const Json* value, std::string_view key) noexcept
{
    return value == nullptr ? nullptr : value->find(key);
}

std::string text(const Json* parent, std::string_view key)
{
    const auto* value = child(parent, key);
    return value == nullptr ? std::string {} : std::string(value->string());
}

void limit(std::string& value, std::size_t maximum)
{
    if (value.size() > maximum)
        value.resize(maximum);
}

bool validStatus(std::string_view value) noexcept
{
    constexpr std::string_view statuses[] {
        "EXPERIMENT", "CANDIDATE", "SELECTED",
        "APPROVED", "REJECTED", "MASTER"};
    return std::find(std::begin(statuses), std::end(statuses), value)
        != std::end(statuses);
}

Json encodeMetadata(const WavMetadata& metadata)
{
    return Json::Object {
        {"title", metadata.title},
        {"artist", metadata.artist},
        {"project", metadata.project},
        {"year", metadata.year},
        {"comment", metadata.comment}
    };
}

WavMetadata decodeMetadata(const Json* value)
{
    if (value == nullptr || !value->isObject())
        return {};
    return {
        text(value, "title"), text(value, "artist"), text(value, "project"),
        text(value, "year"), text(value, "comment")};
}

Json encodeReview(const TakeReview& review)
{
    return Json::Object {
        {"status", review.status}, {"rating", review.rating},
        {"tags", review.tags}, {"notes", review.notes}
    };
}

TakeReview decodeReview(const Json* value)
{
    if (value == nullptr || !value->isObject())
        return {};
    TakeReview review;
    review.status = text(value, "status");
    if (review.status.empty())
        review.status = "EXPERIMENT";
    if (const auto* rating = child(value, "rating"))
        review.rating = static_cast<int>(std::lround(rating->number()));
    review.tags = text(value, "tags");
    review.notes = text(value, "notes");
    return review;
}
}

const std::vector<TakeEntry>& TakeCatalog::entries() const noexcept
{
    return values;
}

TakeEntry* TakeCatalog::find(std::string_view id) noexcept
{
    const auto found = std::find_if(
        values.begin(), values.end(),
        [id] (const auto& entry) { return entry.id == id; });
    return found == values.end() ? nullptr : &*found;
}

const TakeEntry* TakeCatalog::find(std::string_view id) const noexcept
{
    const auto found = std::find_if(
        values.begin(), values.end(),
        [id] (const auto& entry) { return entry.id == id; });
    return found == values.end() ? nullptr : &*found;
}

void TakeCatalog::upsert(TakeEntry entry)
{
    normalizeTakeEntry(entry);
    if (auto* existing = find(entry.id))
    {
        *existing = std::move(entry);
        return;
    }
    if (values.size() >= maximumTakeCatalogEntries)
        throw std::length_error("TAKE catalog entry limit exceeded");
    values.push_back(std::move(entry));
}

bool TakeCatalog::remove(std::string_view id) noexcept
{
    const auto before = values.size();
    std::erase_if(values, [id] (const auto& entry) { return entry.id == id; });
    return values.size() != before;
}

void TakeCatalog::clear() noexcept
{
    values.clear();
}

void normalizeTakeEntry(TakeEntry& entry)
{
    limit(entry.id, 160);
    limit(entry.audioPath, 4096);
    limit(entry.filename, 255);
    limit(entry.createdAt, 64);
    limit(entry.metadata.title, 160);
    limit(entry.metadata.artist, 160);
    limit(entry.metadata.project, 160);
    limit(entry.metadata.year, 12);
    limit(entry.metadata.comment, 500);
    limit(entry.review.status, 16);
    limit(entry.review.tags, 240);
    limit(entry.review.notes, 500);
    if (entry.id.empty() || entry.audioPath.empty() || entry.filename.empty())
        throw std::invalid_argument("TAKE requires id, audio path and filename");
    if (!validStatus(entry.review.status))
        entry.review.status = "EXPERIMENT";
    entry.review.rating = std::clamp(entry.review.rating, 0, 5);
    if (!std::isfinite(entry.durationSeconds) || entry.durationSeconds < 0.0)
        entry.durationSeconds = 0.0;
    if (entry.recipeJson.size() > maximumRecipeBytes)
        throw std::length_error("TAKE recipe is too large");
    if (!entry.recipeJson.empty())
    {
        const auto recipe = parseJson(entry.recipeJson, maximumRecipeBytes, 64);
        if (!recipe.isObject())
            throw std::invalid_argument("TAKE recipe must be a JSON object");
    }
}

std::string encodeTakeCatalog(const TakeCatalog& catalog)
{
    Json::Array takes;
    takes.reserve(catalog.entries().size());
    for (auto entry : catalog.entries())
    {
        normalizeTakeEntry(entry);
        Json recipe;
        if (!entry.recipeJson.empty())
            recipe = parseJson(entry.recipeJson, maximumRecipeBytes, 64);
        takes.emplace_back(Json::Object {
            {"id", entry.id},
            {"audioPath", entry.audioPath},
            {"filename", entry.filename},
            {"createdAt", entry.createdAt},
            {"duration", entry.durationSeconds},
            {"frames", static_cast<double>(entry.frames)},
            {"sampleRate", static_cast<double>(entry.sampleRate)},
            {"sampleFormat", toString(entry.sampleFormat)},
            {"metadata", encodeMetadata(entry.metadata)},
            {"review", encodeReview(entry.review)},
            {"recipe", std::move(recipe)}
        });
    }
    return serializeJson(Json::Object {
        {"format", "navalha-take-catalog"},
        {"version", 1},
        {"appVersion", NAVALHA_JUCE_VERSION},
        {"takes", std::move(takes)}
    }) + "\n";
}

TakeCatalog decodeTakeCatalog(std::string_view json)
{
    const auto root = parseJson(json, maximumCatalogBytes, 64);
    const auto* format = child(&root, "format");
    const auto* version = child(&root, "version");
    const auto* takes = child(&root, "takes");
    if (!root.isObject() || format == nullptr
        || format->string() != "navalha-take-catalog"
        || version == nullptr || version->number() != 1.0
        || takes == nullptr || !takes->isArray())
        throw std::invalid_argument("Unsupported TAKE catalog format or version");
    if (takes->array().size() > maximumTakeCatalogEntries)
        throw std::length_error("TAKE catalog entry limit exceeded");

    TakeCatalog catalog;
    for (const auto& value : takes->array())
    {
        if (!value.isObject())
            throw std::invalid_argument("TAKE catalog entry must be an object");
        TakeEntry entry;
        entry.id = text(&value, "id");
        entry.audioPath = text(&value, "audioPath");
        entry.filename = text(&value, "filename");
        entry.createdAt = text(&value, "createdAt");
        if (const auto* duration = child(&value, "duration"))
            entry.durationSeconds = duration->number();
        if (const auto* frames = child(&value, "frames"))
            entry.frames = static_cast<std::uint64_t>(
                std::max(0.0, frames->number()));
        if (const auto* rate = child(&value, "sampleRate"))
            entry.sampleRate = static_cast<std::uint32_t>(
                std::max(0.0, rate->number()));
        entry.sampleFormat = wavSampleFormatFromString(
            text(&value, "sampleFormat"));
        entry.metadata = decodeMetadata(child(&value, "metadata"));
        entry.review = decodeReview(child(&value, "review"));
        if (const auto* recipe = child(&value, "recipe");
            recipe != nullptr && recipe->isObject())
            entry.recipeJson = serializeJson(*recipe);
        catalog.upsert(std::move(entry));
    }
    return catalog;
}

const char* toString(WavSampleFormat format) noexcept
{
    switch (format)
    {
        case WavSampleFormat::pcm16: return "PCM16";
        case WavSampleFormat::pcm24: return "PCM24";
        case WavSampleFormat::float32: return "FLOAT32";
    }
    return "PCM24";
}

WavSampleFormat wavSampleFormatFromString(
    std::string_view value, WavSampleFormat fallback) noexcept
{
    if (value == "PCM16") return WavSampleFormat::pcm16;
    if (value == "PCM24") return WavSampleFormat::pcm24;
    if (value == "FLOAT32") return WavSampleFormat::float32;
    return fallback;
}
}
