#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/MasteringAlbum.h"
#include "core/MasteringAnalysis.h"
#include "core/TakeCatalog.h"

namespace navalha
{
struct AlbumProjectTrack
{
    std::string id;
    std::string takeId;
    std::string title;
    std::string filename;
    std::string status;
    std::string notes;
    double durationSeconds = 0.0;
    AlbumTrackSettings settings;
    bool hasAnalysis = false;
    MasteringMetrics analysis;
    TakeReview review;
    std::string recipeJson;
};

struct AlbumProject
{
    std::string title = "Untitled album";
    std::string artist;
    std::string notes;
    std::vector<AlbumProjectTrack> tracks;
};

void normalizeAlbumProject(AlbumProject& project);
[[nodiscard]] bool addTakeToAlbumProject(
    AlbumProject& project, const TakeEntry& take);
[[nodiscard]] bool moveAlbumProjectTrack(
    AlbumProject& project, std::size_t index, int offset);
[[nodiscard]] bool removeAlbumProjectTrack(
    AlbumProject& project, std::size_t index) noexcept;
void matchAlbumProjectRelativeLevels(
    AlbumProject& project,
    std::span<const MasteringMetrics> analysis,
    double targetLufs,
    double maximumAbsoluteTrimDb = 6.0);
[[nodiscard]] std::string encodeAlbumProject(
    const AlbumProject& project, std::string_view exportedAt = {});
[[nodiscard]] AlbumProject decodeAlbumProject(std::string_view json);
}
