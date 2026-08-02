#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/MasteringAlbum.h"
#include "core/MasteringAnalysis.h"
#include "core/MasteringProcessor.h"

namespace navalha
{
struct AlbumManifestTrack
{
    std::string id;
    std::string title;
    std::string filename;
    std::string status;
    AlbumTrackSettings settings;
    bool hasAnalysis = false;
    MasteringMetrics analysis;
};

struct AlbumMasterManifest
{
    std::string createdAt;
    std::string title;
    std::string artist;
    std::string notes;
    MasteringParameters chain;
    std::vector<AlbumManifestTrack> tracks;
};

[[nodiscard]] std::string encodeAlbumMasterManifest(
    const AlbumMasterManifest& manifest);
[[nodiscard]] AlbumMasterManifest decodeAlbumMasterManifest(
    std::string_view json);
}
