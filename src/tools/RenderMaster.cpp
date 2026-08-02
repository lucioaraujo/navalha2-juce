#include "core/MasteringAnalysis.h"
#include "core/MasteringProcessor.h"
#include "core/MasteringRecipe.h"
#include "core/WavMemoryReader.h"
#include "core/WavStreamWriter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argumentCount, char** arguments)
{
    std::filesystem::path partialPath;
    bool partialCreated = false;
    try
    {
        if (argumentCount != 3 && argumentCount != 4)
        {
            std::cerr
                << "usage: navalha_render_master INPUT.wav OUTPUT.wav [RECIPE.json]\n";
            return 2;
        }
        constexpr std::uintmax_t maximumInputBytes = 512ULL * 1024ULL * 1024ULL;
        const std::filesystem::path inputPath(arguments[1]);
        const std::filesystem::path outputPath(arguments[2]);
        partialPath = outputPath.string() + ".partial";
        if (std::filesystem::exists(outputPath)
            || std::filesystem::exists(partialPath))
            throw std::runtime_error("Output WAV or partial output already exists");
        const auto size = std::filesystem::file_size(inputPath);
        if (size > maximumInputBytes)
            throw std::length_error("Input WAV exceeds the 512 MiB safety limit");

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        std::ifstream input(inputPath, std::ios::binary);
        if (!input.read(reinterpret_cast<char*>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size())))
            throw std::runtime_error("Unable to read input WAV");
        const auto source = navalha::decodeWav(bytes);
        auto parameters = navalha::MasteringParameters {};
        if (argumentCount == 4)
        {
            constexpr std::uintmax_t maximumRecipeBytes = 1024ULL * 1024ULL;
            const std::filesystem::path recipePath(arguments[3]);
            const auto recipeSize = std::filesystem::file_size(recipePath);
            if (recipeSize > maximumRecipeBytes)
                throw std::length_error(
                    "MASTER recipe exceeds the 1 MiB safety limit");
            std::string recipeJson(static_cast<std::size_t>(recipeSize), '\0');
            std::ifstream recipeInput(recipePath, std::ios::binary);
            if (!recipeInput.read(recipeJson.data(),
                                  static_cast<std::streamsize>(recipeJson.size())))
                throw std::runtime_error("Unable to read MASTER recipe");
            parameters = navalha::decodeMasteringRecipe(recipeJson).parameters;
        }
        const auto rendered = navalha::renderMastering(*source, parameters);

        std::ofstream output(partialPath, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("Unable to create partial output WAV");
        partialCreated = true;
        navalha::WavStreamWriter writer(
            output,
            static_cast<std::uint32_t>(source->sampleRate()),
            navalha::WavSampleFormat::pcm24,
            {"Navalha 2 TRACK MASTER", "", inputPath.filename().string(),
             "", "C++ default mastering chain; internal-estimate-not-EBU-certified"});
        for (std::size_t frame = 0; frame < rendered.left.size(); ++frame)
            writer.writeFrame({rendered.left[frame], rendered.right[frame]});
        writer.finalize();
        output.close();
        if (!output)
            throw std::runtime_error("Unable to finalize output WAV");
        std::filesystem::rename(partialPath, outputPath);
        partialCreated = false;

        const navalha::StereoAudioBuffer mastered(
            source->sampleRate(), rendered.left, rendered.right);
        const auto metrics = navalha::analyzeForMastering(mastered);
        std::cout << "frames=" << mastered.size()
                  << " peak_dbfs=" << metrics.peakDb
                  << " estimated_lufs=" << metrics.estimatedLufs << '\n';
    }
    catch (const std::exception& exception)
    {
        if (partialCreated)
        {
            std::error_code ignored;
            std::filesystem::remove(partialPath, ignored);
        }
        std::cerr << "TRACK MASTER render failed: " << exception.what() << '\n';
        return 1;
    }
}
