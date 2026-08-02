#include "core/MasteringAlbum.h"
#include "core/MasteringAlbumManifest.h"
#include "core/Json.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argumentCount, char** arguments)
{
    try
    {
        if (argumentCount != 2 && argumentCount != 3)
        {
            std::cerr
                << "usage: navalha_inspect_album MANIFEST.json [SAMPLE_RATE]\n";
            return 2;
        }
        constexpr std::uintmax_t maximumManifestBytes = 4ULL * 1024ULL * 1024ULL;
        const std::filesystem::path path(arguments[1]);
        const auto size = std::filesystem::file_size(path);
        if (size > maximumManifestBytes)
            throw std::length_error(
                "ALBUM MASTER manifest exceeds the 4 MiB safety limit");
        std::string json(static_cast<std::size_t>(size), '\0');
        std::ifstream input(path, std::ios::binary);
        if (!input.read(json.data(), static_cast<std::streamsize>(json.size())))
            throw std::runtime_error("Unable to read ALBUM MASTER manifest");
        const auto sampleRate = argumentCount == 3
            ? std::stod(arguments[2]) : 48000.0;
        const auto manifest = navalha::decodeAlbumMasterManifest(json);
        std::vector<navalha::AlbumTrackSettings> settings;
        settings.reserve(manifest.tracks.size());
        for (const auto& track : manifest.tracks)
            settings.push_back(track.settings);
        const auto layout = navalha::planAlbumLayout(settings, sampleRate);
        std::cout << navalha::serializeJson(navalha::Json::Object {
            {"title", manifest.title}, {"artist", manifest.artist},
            {"tracks", static_cast<double>(manifest.tracks.size())},
            {"sample_rate", sampleRate},
            {"total_frames", static_cast<double>(layout.totalFrames)},
            {"duration_seconds",
             static_cast<double>(layout.totalFrames) / sampleRate}
        }) << '\n';
    }
    catch (const std::exception& exception)
    {
        std::cerr << "ALBUM MASTER inspection failed: "
                  << exception.what() << '\n';
        return 1;
    }
}
