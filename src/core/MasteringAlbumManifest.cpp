#include "core/MasteringAlbumManifest.h"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "core/Json.h"
#include "core/MasteringRecipe.h"
#include "core/PortablePath.h"

namespace navalha
{
namespace
{
const Json* child(const Json* value, std::string_view key) noexcept
{
    return value == nullptr ? nullptr : value->find(key);
}

double finite(const Json* value, double fallback)
{
    const auto number = value == nullptr ? fallback : value->number(fallback);
    if (!std::isfinite(number))
        throw std::invalid_argument("ALBUM MASTER contains a non-finite number");
    return number;
}

std::string boundedText(const Json* value,
                        std::string_view fallback = {},
                        std::size_t maximumBytes = 4096)
{
    const auto text = value == nullptr ? fallback : value->string(fallback);
    if (text.size() > maximumBytes)
        throw std::length_error("ALBUM MASTER text field is too large");
    return std::string(text);
}

Json encodeAnalysis(const MasteringMetrics& metrics)
{
    return Json::Object {
        {"peak", metrics.peak}, {"peakDb", metrics.peakDb},
        {"rms", metrics.rms}, {"rmsDb", metrics.rmsDb},
        {"lufs", metrics.estimatedLufs}, {"crest", metrics.crestDb},
        {"corr", metrics.correlation}, {"headroom", metrics.headroomDb}
    };
}

MasteringMetrics decodeAnalysis(const Json& value)
{
    MasteringMetrics metrics;
    metrics.peak = finite(child(&value, "peak"), 0.0);
    metrics.peakDb = finite(child(&value, "peakDb"), -240.0);
    metrics.rms = finite(child(&value, "rms"), 0.0);
    metrics.rmsDb = finite(child(&value, "rmsDb"), -240.0);
    metrics.estimatedLufs = finite(child(&value, "lufs"), -120.691);
    metrics.crestDb = finite(child(&value, "crest"), 0.0);
    metrics.correlation = finite(child(&value, "corr"), 0.0);
    metrics.headroomDb = finite(child(&value, "headroom"), 240.0);
    return metrics;
}

void validateSettings(const AlbumTrackSettings& settings)
{
    const std::array<AlbumTrackSettings, 1> oneTrack {settings};
    static_cast<void>(planAlbumLayout(oneTrack, 48000.0));
}
}

std::string encodeAlbumMasterManifest(const AlbumMasterManifest& manifest)
{
    if (manifest.tracks.size() > maximumAlbumTracks)
        throw std::length_error("Album exceeds the 99-track limit");
    Json::Array tracks;
    for (std::size_t index = 0; index < manifest.tracks.size(); ++index)
    {
        const auto& track = manifest.tracks[index];
        validateSettings(track.settings);
        if (!track.filename.empty()
            && !isSafePortableRelativePath(track.filename))
            throw std::invalid_argument("Unsafe ALBUM MASTER track filename");
        tracks.emplace_back(Json::Object {
            {"order", static_cast<double>(index + 1)},
            {"id", track.id}, {"title", track.title},
            {"filename", track.filename}, {"status", track.status},
            {"duration", track.settings.durationSeconds},
            {"trim", track.settings.trimDb},
            {"gapAfter", track.settings.gapAfterSeconds},
            {"fadeIn", track.settings.fadeInSeconds},
            {"fadeOut", track.settings.fadeOutSeconds},
            {"analysis", track.hasAnalysis ? encodeAnalysis(track.analysis)
                                           : Json(nullptr)}
        });
    }
    return serializeJson(Json::Object {
        {"format", "navalha-album-master"}, {"version", 1},
        {"appVersion", "0.28.1"}, {"createdAt", manifest.createdAt},
        {"metering", "internal-estimate-not-EBU-certified"},
        {"chain", encodeMasteringParameters(manifest.chain)},
        {"album", Json::Object {
            {"title", manifest.title}, {"artist", manifest.artist},
            {"notes", manifest.notes}
        }},
        {"tracks", std::move(tracks)}
    }) + "\n";
}

AlbumMasterManifest decodeAlbumMasterManifest(std::string_view json)
{
    constexpr std::size_t maximumManifestBytes = 4 * 1024 * 1024;
    const auto root = parseJson(json, maximumManifestBytes, 24);
    if (!root.isObject() || child(&root, "format") == nullptr
        || child(&root, "format")->string() != "navalha-album-master"
        || child(&root, "version") == nullptr
        || child(&root, "version")->number() != 1.0)
        throw std::invalid_argument(
            "Unsupported ALBUM MASTER manifest format or version");
    const auto* chain = child(&root, "chain");
    const auto* album = child(&root, "album");
    const auto* tracks = child(&root, "tracks");
    if (chain == nullptr || album == nullptr || !album->isObject()
        || tracks == nullptr || !tracks->isArray())
        throw std::invalid_argument("Incomplete ALBUM MASTER manifest");
    if (tracks->array().size() > maximumAlbumTracks)
        throw std::length_error("Album exceeds the 99-track limit");

    AlbumMasterManifest result;
    result.createdAt = boundedText(child(&root, "createdAt"));
    result.title = boundedText(child(album, "title"), "Untitled album");
    result.artist = boundedText(child(album, "artist"));
    result.notes = boundedText(child(album, "notes"), {}, 64 * 1024);
    result.chain = decodeMasteringParameters(*chain);
    for (std::size_t index = 0; index < tracks->array().size(); ++index)
    {
        const auto& value = tracks->array()[index];
        if (!value.isObject())
            throw std::invalid_argument("ALBUM MASTER track must be an object");
        AlbumManifestTrack track;
        track.id = boundedText(child(&value, "id"),
                               "track-" + std::to_string(index + 1));
        track.title = boundedText(child(&value, "title"),
                                  "Track " + std::to_string(index + 1));
        track.filename = boundedText(child(&value, "filename"));
        track.status = boundedText(child(&value, "status"));
        if (!track.filename.empty()
            && !isSafePortableRelativePath(track.filename))
            throw std::invalid_argument("Unsafe ALBUM MASTER track filename");
        track.settings = {
            finite(child(&value, "duration"), 0.0),
            finite(child(&value, "trim"), 0.0),
            finite(child(&value, "gapAfter"), 2.0),
            finite(child(&value, "fadeIn"), 0.0),
            finite(child(&value, "fadeOut"), 0.0)
        };
        validateSettings(track.settings);
        if (const auto* analysis = child(&value, "analysis");
            analysis != nullptr && analysis->isObject())
        {
            track.analysis = decodeAnalysis(*analysis);
            track.hasAnalysis = true;
        }
        result.tracks.push_back(std::move(track));
    }
    return result;
}
}
