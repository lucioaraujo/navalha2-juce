#include "core/AlbumProject.h"

#include <algorithm>
#include <array>
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
constexpr std::size_t maximumAlbumProjectBytes = 8 * 1024 * 1024;
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

void normalizeTrack(AlbumProjectTrack& track)
{
    limit(track.id, 160);
    limit(track.takeId, 160);
    limit(track.title, 160);
    limit(track.filename, 255);
    limit(track.status, 16);
    limit(track.notes, 500);
    limit(track.review.status, 16);
    limit(track.review.tags, 240);
    limit(track.review.notes, 500);
    if (track.id.empty() || track.takeId.empty() || track.filename.empty())
        throw std::invalid_argument(
            "ALBUM PROJECT track requires id, take id and filename");
    if (track.title.empty())
        track.title = track.filename;
    if (!validStatus(track.status))
        track.status = "EXPERIMENT";
    if (!validStatus(track.review.status))
        track.review.status = track.status;
    track.review.rating = std::clamp(track.review.rating, 0, 5);
    if (!std::isfinite(track.durationSeconds)
        || track.durationSeconds < 0.0)
        track.durationSeconds = 0.0;
    track.settings.durationSeconds = track.durationSeconds;
    const std::array<AlbumTrackSettings, 1> settings {track.settings};
    static_cast<void>(planAlbumLayout(settings, 48000.0));
    if (track.recipeJson.size() > maximumRecipeBytes)
        throw std::length_error("ALBUM PROJECT recipe is too large");
    if (!track.recipeJson.empty())
    {
        const auto recipe = parseJson(track.recipeJson, maximumRecipeBytes, 64);
        if (!recipe.isObject())
            throw std::invalid_argument(
                "ALBUM PROJECT recipe must be a JSON object");
    }
}

Json encodeReview(const TakeReview& review)
{
    return Json::Object {
        {"status", review.status}, {"rating", review.rating},
        {"tags", review.tags}, {"notes", review.notes}
    };
}

TakeReview decodeReview(const Json* value, std::string_view fallbackStatus)
{
    TakeReview review;
    review.status = std::string(fallbackStatus);
    if (value == nullptr || !value->isObject())
        return review;
    if (const auto decoded = text(value, "status"); !decoded.empty())
        review.status = decoded;
    if (const auto* rating = child(value, "rating"))
        review.rating = static_cast<int>(std::lround(rating->number()));
    review.tags = text(value, "tags");
    review.notes = text(value, "notes");
    return review;
}

double finite(const Json* value, double fallback)
{
    const auto number = value == nullptr ? fallback : value->number(fallback);
    if (!std::isfinite(number))
        throw std::invalid_argument(
            "ALBUM PROJECT contains a non-finite number");
    return number;
}
}

void normalizeAlbumProject(AlbumProject& project)
{
    limit(project.title, 120);
    limit(project.artist, 120);
    limit(project.notes, 500);
    if (project.title.empty())
        project.title = "Untitled album";
    if (project.tracks.size() > maximumAlbumTracks)
        throw std::length_error("Album exceeds the 99-track limit");
    for (auto& track : project.tracks)
        normalizeTrack(track);
}

bool addTakeToAlbumProject(AlbumProject& project, const TakeEntry& take)
{
    if (std::any_of(
            project.tracks.begin(), project.tracks.end(),
            [&take] (const auto& track) { return track.takeId == take.id; }))
        return false;
    if (project.tracks.size() >= maximumAlbumTracks)
        throw std::length_error("Album exceeds the 99-track limit");
    AlbumProjectTrack track;
    track.id = take.id;
    track.takeId = take.id;
    track.title = take.metadata.title.empty()
        ? take.filename : take.metadata.title;
    track.filename = take.filename;
    track.status = take.review.status;
    track.notes = take.review.notes;
    track.durationSeconds = take.durationSeconds;
    track.settings.durationSeconds = take.durationSeconds;
    track.review = take.review;
    track.recipeJson = take.recipeJson;
    normalizeTrack(track);
    project.tracks.push_back(std::move(track));
    if ((project.title.empty() || project.title == "Untitled album")
        && !take.metadata.project.empty())
        project.title = take.metadata.project;
    if (project.artist.empty() && !take.metadata.artist.empty())
        project.artist = take.metadata.artist;
    normalizeAlbumProject(project);
    return true;
}

bool moveAlbumProjectTrack(
    AlbumProject& project, std::size_t index, int offset)
{
    if (index >= project.tracks.size() || offset == 0)
        return false;
    const auto last = static_cast<long long>(project.tracks.size() - 1);
    const auto target = std::clamp(
        static_cast<long long>(index) + static_cast<long long>(offset),
        0LL, last);
    if (target == static_cast<long long>(index))
        return false;
    auto track = std::move(project.tracks[index]);
    project.tracks.erase(
        project.tracks.begin() + static_cast<std::ptrdiff_t>(index));
    project.tracks.insert(
        project.tracks.begin() + static_cast<std::ptrdiff_t>(target),
        std::move(track));
    return true;
}

bool removeAlbumProjectTrack(
    AlbumProject& project, std::size_t index) noexcept
{
    if (index >= project.tracks.size())
        return false;
    project.tracks.erase(
        project.tracks.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

std::string encodeAlbumProject(
    const AlbumProject& source, std::string_view exportedAt)
{
    auto project = source;
    normalizeAlbumProject(project);
    Json::Array tracks;
    tracks.reserve(project.tracks.size());
    for (std::size_t index = 0; index < project.tracks.size(); ++index)
    {
        const auto& track = project.tracks[index];
        Json recipe;
        if (!track.recipeJson.empty())
            recipe = parseJson(track.recipeJson, maximumRecipeBytes, 64);
        tracks.emplace_back(Json::Object {
            {"order", static_cast<double>(index + 1)},
            {"id", track.id}, {"takeKey", track.takeId},
            {"filename", track.filename}, {"title", track.title},
            {"duration", track.durationSeconds}, {"status", track.status},
            {"notes", track.notes},
            {"trim", track.settings.trimDb},
            {"gapAfter", track.settings.gapAfterSeconds},
            {"fadeIn", track.settings.fadeInSeconds},
            {"fadeOut", track.settings.fadeOutSeconds},
            {"recipe", std::move(recipe)},
            {"review", encodeReview(track.review)}
        });
    }
    auto encoded = serializeJson(Json::Object {
        {"format", "navalha-album-project"}, {"version", 1},
        {"appVersion", NAVALHA_JUCE_VERSION},
        {"exportedAt", std::string(exportedAt)},
        {"title", project.title}, {"artist", project.artist},
        {"notes", project.notes}, {"tracks", std::move(tracks)}
    }) + "\n";
    if (encoded.size() > maximumAlbumProjectBytes)
        throw std::length_error("ALBUM PROJECT exceeds the 8 MiB limit");
    return encoded;
}

AlbumProject decodeAlbumProject(std::string_view json)
{
    const auto root = parseJson(json, maximumAlbumProjectBytes, 64);
    const auto* tracks = child(&root, "tracks");
    if (!root.isObject() || child(&root, "format") == nullptr
        || child(&root, "format")->string() != "navalha-album-project"
        || child(&root, "version") == nullptr
        || child(&root, "version")->number() != 1.0
        || tracks == nullptr || !tracks->isArray())
        throw std::invalid_argument(
            "Unsupported ALBUM PROJECT format or version");
    if (tracks->array().size() > maximumAlbumTracks)
        throw std::length_error("Album exceeds the 99-track limit");
    AlbumProject project;
    project.title = text(&root, "title");
    project.artist = text(&root, "artist");
    project.notes = text(&root, "notes");
    for (std::size_t index = 0; index < tracks->array().size(); ++index)
    {
        const auto& value = tracks->array()[index];
        if (!value.isObject())
            throw std::invalid_argument(
                "ALBUM PROJECT track must be an object");
        AlbumProjectTrack track;
        track.id = text(&value, "id");
        track.takeId = text(&value, "takeId");
        if (track.takeId.empty())
            track.takeId = text(&value, "takeKey");
        if (track.id.empty())
            track.id = track.takeId.empty()
                ? "track-" + std::to_string(index + 1) : track.takeId;
        track.title = text(&value, "title");
        track.filename = text(&value, "filename");
        track.status = text(&value, "status");
        track.notes = text(&value, "notes");
        track.durationSeconds = finite(child(&value, "duration"), 0.0);
        track.settings = {
            track.durationSeconds,
            finite(child(&value, "trim"), 0.0),
            finite(child(&value, "gapAfter"), 2.0),
            finite(child(&value, "fadeIn"), 0.0),
            finite(child(&value, "fadeOut"), 0.0)};
        track.review = decodeReview(child(&value, "review"), track.status);
        if (const auto* recipe = child(&value, "recipe");
            recipe != nullptr && recipe->isObject())
            track.recipeJson = serializeJson(*recipe);
        project.tracks.push_back(std::move(track));
    }
    normalizeAlbumProject(project);
    return project;
}
}
