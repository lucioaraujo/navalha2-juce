#include "core/AudioEngine.h"
#include "core/PortableProject.h"
#include "core/ProjectState.h"
#include "core/WavStreamWriter.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
std::vector<std::uint8_t> readPortable(const std::filesystem::path& path)
{
    constexpr std::uintmax_t maximumBytes = 256ULL * 1024ULL * 1024ULL;
    const auto size = std::filesystem::file_size(path);
    if (size > maximumBytes)
        throw std::length_error("Portable project exceeds the 256 MiB safety limit");

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    if (!input.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Unable to read portable project");
    return bytes;
}

void updateChecksum(std::uint64_t& checksum, float sample)
{
    checksum ^= std::bit_cast<std::uint32_t>(sample);
    checksum *= 1099511628211ULL;
}
}

int main(int argumentCount, char** arguments)
{
    if (argumentCount != 4 && argumentCount != 5)
    {
        std::cerr
            << "usage: navalha_render_portable PROJECT.zip OUTPUT.wav"
               " SECONDS [SAMPLE_RATE]\n";
        return 2;
    }

    std::filesystem::path partialOutputPath;
    bool partialOutputCreated = false;
    try
    {
        const std::filesystem::path projectPath(arguments[1]);
        const std::filesystem::path outputPath(arguments[2]);
        partialOutputPath = outputPath.string() + ".partial";
        if (std::filesystem::exists(outputPath)
            || std::filesystem::exists(partialOutputPath))
            throw std::runtime_error("Output WAV or partial output already exists");

        const auto seconds = std::stod(arguments[3]);
        const auto sampleRate = argumentCount == 5
            ? std::stod(arguments[4]) : 48000.0;
        if (!std::isfinite(seconds) || seconds <= 0.0 || seconds > 600.0)
            throw std::out_of_range("Duration must be between 0 and 600 seconds");
        if (!std::isfinite(sampleRate)
            || sampleRate < 8000.0 || sampleRate > 192000.0)
            throw std::out_of_range("Sample rate must be between 8000 and 192000 Hz");

        const auto archive = readPortable(projectPath);
        auto loaded = navalha::openPortableProject(archive);
        if (loaded.sourceA == nullptr && loaded.sourceB == nullptr)
            throw std::runtime_error("Portable project contains no source audio");

        navalha::SessionModel session;
        navalha::restoreProjectState(loaded.project, session);
        navalha::AudioEngine engine(session);
        engine.prepare(sampleRate);
        engine.setSourceBuffer(0, loaded.sourceA.get());
        engine.setSourceBuffer(1, loaded.sourceB.get());
        if (!engine.submitCommand({navalha::EngineCommandType::start}))
            throw std::runtime_error("Unable to start offline transport");

        std::ofstream output(
            partialOutputPath, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("Unable to create output WAV");
        partialOutputCreated = true;
        navalha::WavStreamWriter writer(
            output,
            static_cast<std::uint32_t>(std::lround(sampleRate)),
            navalha::WavSampleFormat::pcm24,
            {"Navalha 2 offline render", "Navalha 2", projectPath.filename().string(),
             "", "JUCE deterministic portable-project render"});

        constexpr std::size_t blockSize = 512;
        std::vector<float> left(blockSize);
        std::vector<float> right(blockSize);
        const auto totalFrames = static_cast<std::uint64_t>(
            std::llround(seconds * sampleRate));
        std::uint64_t renderedFrames = 0;
        std::uint64_t checksum = 14695981039346656037ULL;
        double peak = 0.0;
        long double squareSum = 0.0L;

        while (renderedFrames < totalFrames)
        {
            const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(
                blockSize, totalFrames - renderedFrames));
            engine.processBlock(left.data(), right.data(), count);
            for (std::size_t frame = 0; frame < count; ++frame)
            {
                writer.writeFrame({left[frame], right[frame]});
                peak = std::max({
                    peak, std::abs(static_cast<double>(left[frame])),
                    std::abs(static_cast<double>(right[frame]))});
                squareSum += static_cast<long double>(left[frame]) * left[frame]
                    + static_cast<long double>(right[frame]) * right[frame];
                updateChecksum(checksum, left[frame]);
                updateChecksum(checksum, right[frame]);
            }
            renderedFrames += count;
        }
        writer.finalize();
        output.close();
        if (!output)
            throw std::runtime_error("Unable to close output WAV");
        std::filesystem::rename(partialOutputPath, outputPath);
        partialOutputCreated = false;

        const auto rms = renderedFrames == 0 ? 0.0 : std::sqrt(
            static_cast<double>(
                squareSum / static_cast<long double>(renderedFrames * 2)));
        std::cout << "{\n"
                  << "  \"frames\": " << renderedFrames << ",\n"
                  << "  \"sample_rate\": "
                  << static_cast<std::uint32_t>(std::lround(sampleRate)) << ",\n"
                  << "  \"peak\": " << peak << ",\n"
                  << "  \"rms\": " << rms << ",\n"
                  << "  \"checksum\": " << checksum << "\n"
                  << "}\n";
    }
    catch (const std::exception& exception)
    {
        if (partialOutputCreated)
        {
            std::error_code ignoredError;
            std::filesystem::remove(partialOutputPath, ignoredError);
        }
        std::cerr << "render failed: " << exception.what() << '\n';
        return 1;
    }
}
