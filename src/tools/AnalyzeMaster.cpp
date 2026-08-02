#include "core/MasteringAnalysis.h"
#include "core/WavMemoryReader.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argumentCount, char** arguments)
{
    if (argumentCount != 2)
    {
        std::cerr << "usage: navalha_analyze_master INPUT.wav\n";
        return 2;
    }

    try
    {
        constexpr std::uintmax_t maximumInputBytes = 512ULL * 1024ULL * 1024ULL;
        const std::filesystem::path path(arguments[1]);
        const auto size = std::filesystem::file_size(path);
        if (size > maximumInputBytes)
            throw std::length_error("Input WAV exceeds the 512 MiB safety limit");

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        std::ifstream input(path, std::ios::binary);
        if (!input.read(reinterpret_cast<char*>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size())))
            throw std::runtime_error("Unable to read input WAV");

        const auto audio = navalha::decodeWav(bytes);
        const auto metrics = navalha::analyzeForMastering(*audio);
        std::cout << std::fixed << std::setprecision(6)
                  << "{\n"
                  << "  \"metering\": \"internal-estimate-not-EBU-certified\",\n"
                  << "  \"frames\": " << audio->size() << ",\n"
                  << "  \"sample_rate\": " << audio->sampleRate() << ",\n"
                  << "  \"inspected_frames\": " << metrics.inspectedFrames << ",\n"
                  << "  \"peak\": " << metrics.peak << ",\n"
                  << "  \"peak_dbfs\": " << metrics.peakDb << ",\n"
                  << "  \"rms\": " << metrics.rms << ",\n"
                  << "  \"rms_dbfs\": " << metrics.rmsDb << ",\n"
                  << "  \"estimated_lufs\": " << metrics.estimatedLufs << ",\n"
                  << "  \"crest_db\": " << metrics.crestDb << ",\n"
                  << "  \"correlation\": " << metrics.correlation << ",\n"
                  << "  \"headroom_db\": " << metrics.headroomDb << "\n"
                  << "}\n";
    }
    catch (const std::exception& exception)
    {
        std::cerr << "MASTER analysis failed: " << exception.what() << '\n';
        return 1;
    }
}
