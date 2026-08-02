#include "core/MasteringAlbumManifest.h"
#include "core/MasteringProcessor.h"
#include "core/WavMemoryReader.h"
#include "core/WavStreamWriter.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
constexpr std::uintmax_t maximumManifestBytes = 4ULL * 1024ULL * 1024ULL;
constexpr std::uintmax_t maximumTrackBytes = 512ULL * 1024ULL * 1024ULL;
constexpr std::uintmax_t diskReserveBytes = 1024ULL * 1024ULL * 1024ULL;

std::string readText(const std::filesystem::path& path, std::uintmax_t limit)
{
    const auto size = std::filesystem::file_size(path);
    if (size > limit)
        throw std::length_error("Input exceeds its safety limit");
    std::string text(static_cast<std::size_t>(size), '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(text.data(), static_cast<std::streamsize>(text.size())))
        throw std::runtime_error("Unable to read input");
    return text;
}

std::unique_ptr<navalha::StereoAudioBuffer> readAudio(
    const std::filesystem::path& path)
{
    const auto size = std::filesystem::file_size(path);
    if (size > maximumTrackBytes)
        throw std::length_error("Album track exceeds the 512 MiB safety limit");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    if (!input.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Unable to read album track");
    return navalha::decodeWav(bytes);
}

std::string safeStem(std::string_view title)
{
    std::string result;
    result.reserve(std::min<std::size_t>(title.size(), 80));
    for (const auto character : title)
    {
        if (result.size() == 80)
            break;
        const auto byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) != 0 || character == '-' || character == '_')
            result.push_back(character);
        else if (!result.empty() && result.back() != '_')
            result.push_back('_');
    }
    while (!result.empty() && result.back() == '_')
        result.pop_back();
    return result.empty() ? "track" : result;
}
}

int main(int argumentCount, char** arguments)
{
    std::filesystem::path activePartial;
    try
    {
        if (argumentCount != 3)
        {
            std::cerr
                << "usage: navalha_render_album MANIFEST.json OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path manifestPath =
            std::filesystem::absolute(arguments[1]).lexically_normal();
        const std::filesystem::path outputDirectory =
            std::filesystem::absolute(arguments[2]).lexically_normal();
        if (!std::filesystem::is_directory(outputDirectory))
            throw std::invalid_argument("Output directory must already exist");
        const auto manifest = navalha::decodeAlbumMasterManifest(
            readText(manifestPath, maximumManifestBytes));
        if (manifest.tracks.empty())
            throw std::invalid_argument("ALBUM MASTER contains no tracks");

        std::vector<std::filesystem::path> sources;
        std::vector<std::filesystem::path> outputs;
        std::uintmax_t sourceBytes = 0;
        for (std::size_t index = 0; index < manifest.tracks.size(); ++index)
        {
            const auto& track = manifest.tracks[index];
            if (track.filename.empty())
                throw std::invalid_argument("ALBUM MASTER track has no filename");
            const auto source =
                (manifestPath.parent_path() / track.filename).lexically_normal();
            if (!std::filesystem::is_regular_file(source))
                throw std::runtime_error(
                    "Missing album track: " + track.filename);
            const auto size = std::filesystem::file_size(source);
            if (size > maximumTrackBytes
                || sourceBytes > std::numeric_limits<std::uintmax_t>::max() - size)
                throw std::length_error("Album source set is too large");
            sourceBytes += size;
            sources.push_back(source);

            std::ostringstream prefix;
            prefix << std::setw(2) << std::setfill('0') << index + 1 << '_';
            const auto output = outputDirectory
                / (prefix.str() + safeStem(track.title) + "_MASTER.wav");
            if (std::filesystem::exists(output)
                || std::filesystem::exists(output.string() + ".partial"))
                throw std::runtime_error(
                    "Album output or partial output already exists");
            outputs.push_back(output);
        }
        const auto available = std::filesystem::space(outputDirectory).available;
        if (sourceBytes > (available > diskReserveBytes
                ? (available - diskReserveBytes) / 4 : 0))
            throw std::runtime_error(
                "Insufficient disk space while preserving the 1 GiB reserve");

        // Decode and validate every input before publishing the first track.
        for (std::size_t index = 0; index < sources.size(); ++index)
        {
            const auto source = readAudio(sources[index]);
            auto parameters = manifest.chain;
            parameters.trimDb += manifest.tracks[index].settings.trimDb;
            navalha::MasteringProcessor processor;
            processor.prepare(source->sampleRate(), parameters);
        }

        for (std::size_t index = 0; index < manifest.tracks.size(); ++index)
        {
            const auto source = readAudio(sources[index]);
            auto parameters = manifest.chain;
            parameters.trimDb += manifest.tracks[index].settings.trimDb;
            const auto rendered = navalha::renderMastering(*source, parameters);

            activePartial = outputs[index].string() + ".partial";
            std::ofstream output(
                activePartial, std::ios::binary | std::ios::trunc);
            if (!output)
                throw std::runtime_error("Unable to create partial album output");
            navalha::WavStreamWriter writer(
                output, static_cast<std::uint32_t>(source->sampleRate()),
                navalha::WavSampleFormat::pcm24,
                {manifest.tracks[index].title, manifest.artist, manifest.title,
                 "", "Navalha 2 C++ ALBUM MASTER batch render"});
            for (std::size_t frame = 0; frame < rendered.left.size(); ++frame)
                writer.writeFrame({rendered.left[frame], rendered.right[frame]});
            writer.finalize();
            output.close();
            if (!output)
                throw std::runtime_error("Unable to finalize album output");
            std::filesystem::rename(activePartial, outputs[index]);
            activePartial.clear();
            std::cout << "rendered " << index + 1 << '/'
                      << manifest.tracks.size() << ": "
                      << outputs[index].filename().string() << '\n';
        }
    }
    catch (const std::exception& exception)
    {
        if (!activePartial.empty())
        {
            std::error_code ignored;
            std::filesystem::remove(activePartial, ignored);
        }
        std::cerr << "ALBUM MASTER render failed: "
                  << exception.what() << '\n';
        return 1;
    }
}
