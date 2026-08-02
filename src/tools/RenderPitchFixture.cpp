#include "core/HeritagePitch.h"
#include "core/WavStreamWriter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argumentCount, char** arguments)
{
    if (argumentCount != 3)
    {
        std::cerr << "usage: navalha_render_pitch_fixture OUTPUT.wav SEMITONES\n";
        return 2;
    }

    try
    {
        constexpr double sampleRate = 48000.0;
        constexpr std::uint64_t frameCount = 48000;
        const std::filesystem::path outputPath(arguments[1]);
        const auto semitones = std::stoi(arguments[2]);
        if (semitones < -12 || semitones > 11)
            throw std::out_of_range("Semitones must be between -12 and 11");
        if (std::filesystem::exists(outputPath))
            throw std::runtime_error("Output WAV already exists");

        navalha::LegacyPitchChannel pitch;
        pitch.prepare(sampleRate);
        pitch.setSemitones(semitones);

        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("Unable to create output WAV");
        navalha::WavStreamWriter writer(
            output, 48000, navalha::WavSampleFormat::float32);

        double peak = 0.0;
        for (std::uint64_t frame = 0; frame < frameCount; ++frame)
        {
            const auto sample = pitch.process(frame == 0 ? 1.0F : 0.0F);
            writer.writeFrame({sample, sample});
            peak = std::max(peak, std::abs(static_cast<double>(sample)));
        }
        writer.finalize();
        std::cout << "frames=" << frameCount << " peak=" << peak << '\n';
    }
    catch (const std::exception& exception)
    {
        std::cerr << "pitch fixture render failed: " << exception.what() << '\n';
        return 1;
    }
}
