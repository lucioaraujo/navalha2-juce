#include "core/LookaheadLimiter.h"
#include "core/TruePeakDetector.h"
#include "core/WavStreamWriter.h"
#include "validation/TruePeakFixtures.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr std::uint32_t sampleRate = 48000;

double measureDbtp(const std::vector<float>& signal)
{
    navalha::TruePeakDetector detector;
    detector.prepare(sampleRate);
    double peak = 0.0;
    for (const auto sample : signal)
        peak = std::max(
            peak, static_cast<double>(detector.processSample(sample)));
    return 20.0 * std::log10(peak);
}

void writeFloatWav(const std::filesystem::path& path,
                   const std::vector<float>& signal,
                   int caseNumber,
                   const std::string& stage)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("Cannot create fixture: " + path.string());
    navalha::WavStreamWriter writer(
        output, sampleRate, navalha::WavSampleFormat::float32,
        {"EBU Tech 3341 case " + std::to_string(caseNumber),
         "Navalha 2 validation", "Output safety P0.3", "2026",
         stage + "; validation fixture, do not monitor at normal volume"});
    for (const auto sample : signal)
        writer.writeFrame({sample, sample});
    writer.finalize();
}
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2)
            throw std::invalid_argument(
                "Usage: navalha_render_true_peak_fixtures OUTPUT_DIRECTORY");
        const std::filesystem::path outputDirectory(argv[1]);
        std::filesystem::create_directories(outputDirectory);

        std::cout << "case\texpected\tinternal input\tinternal limited\tGR\n";
        for (const auto& fixture :
             navalha::validation::makeEbuTruePeakFixtures())
        {
            navalha::LookaheadLimiter limiter;
            limiter.prepare(sampleRate);
            std::vector<float> limited;
            limited.reserve(
                fixture.samples.size() + limiter.latencySamples() + 128);
            for (const auto sample : fixture.samples)
                limited.push_back(limiter.process({sample, sample}).left);
            for (std::size_t frame = 0;
                 frame < limiter.latencySamples() + 128; ++frame)
                limited.push_back(limiter.process({}).left);

            const auto base = "ebu3341_case"
                + std::to_string(fixture.caseNumber)
                + (fixture.derivedTransient ? "_derived" : "");
            writeFloatWav(
                outputDirectory / (base + "_input_f32.wav"),
                fixture.samples, fixture.caseNumber, "input");
            writeFloatWav(
                outputDirectory / (base + "_limited_f32.wav"),
                limited, fixture.caseNumber, "limited at -1 dBTP");
            const auto telemetry = limiter.telemetry();
            std::cout << fixture.caseNumber << '\t'
                      << std::fixed << std::setprecision(2)
                      << fixture.expectedDbtp << '\t'
                      << measureDbtp(fixture.samples) << '\t'
                      << measureDbtp(limited) << '\t'
                      << telemetry.gainReductionDb << '\n';
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "True-peak fixture export failed: "
                  << error.what() << '\n';
        return 1;
    }
}
