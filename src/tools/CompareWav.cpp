#include "core/AudioComparison.h"
#include "core/WavMemoryReader.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
    constexpr std::uintmax_t maximumBytes = 512ULL * 1024ULL * 1024ULL;
    const auto size = std::filesystem::file_size(path);
    if (size > maximumBytes)
        throw std::length_error("Comparison WAV exceeds the 512 MiB safety limit");

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    if (!input.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Unable to read comparison WAV");
    return bytes;
}
}

int main(int argumentCount, char** arguments)
{
    if (argumentCount != 3 && argumentCount != 5)
    {
        std::cerr
            << "usage: navalha_compare_wav REFERENCE.wav CANDIDATE.wav"
               " [--candidate-offset FRAMES]\n";
        return 2;
    }

    try
    {
        std::ptrdiff_t candidateOffset = 0;
        const auto aligned = argumentCount == 5;
        if (aligned)
        {
            if (std::string(arguments[3]) != "--candidate-offset")
                throw std::invalid_argument("Unknown comparison option");
            const auto parsed = std::stoll(arguments[4]);
            if (parsed < std::numeric_limits<std::ptrdiff_t>::min()
                || parsed > std::numeric_limits<std::ptrdiff_t>::max())
                throw std::out_of_range("Candidate offset is outside the safe range");
            candidateOffset = static_cast<std::ptrdiff_t>(parsed);
        }
        const auto referenceBytes = readFile(arguments[1]);
        const auto candidateBytes = readFile(arguments[2]);
        const auto reference = navalha::decodeWav(referenceBytes);
        const auto candidate = navalha::decodeWav(candidateBytes);
        const auto comparison = aligned
            ? navalha::compareAudioAligned(
                *reference, *candidate, candidateOffset)
            : navalha::compareAudio(*reference, *candidate);

        std::cout << std::setprecision(12)
                  << "{\n"
                  << "  \"candidate_offset_frames\": "
                  << candidateOffset << ",\n"
                  << "  \"frames\": " << comparison.frames << ",\n"
                  << "  \"reference_rms\": " << comparison.referenceRms << ",\n"
                  << "  \"difference_rms\": " << comparison.differenceRms << ",\n"
                  << "  \"maximum_absolute_difference\": "
                  << comparison.maximumAbsoluteDifference << ",\n"
                  << "  \"correlation\": " << comparison.correlation << ",\n"
                  << "  \"snr_db\": ";
        if (std::isinf(comparison.signalToNoiseDb))
            std::cout << "\"infinity\"";
        else
            std::cout << comparison.signalToNoiseDb;
        std::cout << "\n}\n";
    }
    catch (const std::exception& exception)
    {
        std::cerr << "comparison failed: " << exception.what() << '\n';
        return 1;
    }
}
