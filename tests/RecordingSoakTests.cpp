#include "core/AudioEngine.h"
#include "core/RecordingWriterService.h"
#include "core/SessionModel.h"
#include "core/SlicePlayer.h"
#include "core/WavMemoryReader.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
class TemporaryRecording
{
public:
    explicit TemporaryRecording(std::filesystem::path newPath)
        : path(std::move(newPath))
    {
    }

    ~TemporaryRecording()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        const auto prefix = "." + path.filename().string() + ".";
        for (const auto& entry :
             std::filesystem::directory_iterator(path.parent_path(), ignored))
        {
            const auto name = entry.path().filename().string();
            if (name.starts_with(prefix) && name.ends_with(".partial"))
                std::filesystem::remove(entry.path(), ignored);
        }
    }

    std::filesystem::path path;
};

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    const auto size = std::filesystem::file_size(path);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    if (!input.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Unable to reopen recording");
    return bytes;
}
}

int main(int argc, char** argv)
{
    try
    {
        std::uint64_t seconds = 5;
        if (argc == 3 && std::string(argv[1]) == "--seconds")
            seconds = std::stoull(argv[2]);
        else if (argc != 1)
            throw std::invalid_argument(
                "Usage: navalha_recording_soak_tests [--seconds 1..600]");
        if (seconds < 1 || seconds > 600)
            throw std::invalid_argument(
                "Recording soak duration must be between 1 and 600 seconds");

        constexpr std::uint32_t sampleRate = 48000;
        const auto targetFrames = seconds * sampleRate;
        std::vector<float> sourceLeft(sampleRate);
        std::vector<float> sourceRight(sampleRate);
        for (std::size_t frame = 0; frame < sourceLeft.size(); ++frame)
        {
            const auto time = static_cast<double>(frame) / sampleRate;
            sourceLeft[frame] = static_cast<float>(
                0.3 * std::sin(2.0 * std::numbers::pi * 220.0 * time));
            sourceRight[frame] = static_cast<float>(
                0.3 * std::sin(2.0 * std::numbers::pi * 330.0 * time));
        }
        navalha::StereoAudioBuffer source(
            sampleRate, std::move(sourceLeft), std::move(sourceRight));
        navalha::SessionModel session;
        for (std::size_t step = 0; step < navalha::stepsPerPattern; ++step)
            session.patterns.setCell(0, step, 0);
        navalha::AudioEngine engine(session);
        engine.prepare(sampleRate);
        engine.setSourceBuffer(0, &source);
        if (!engine.submitCommand({navalha::EngineCommandType::start}))
            throw std::runtime_error("Unable to start recording soak transport");

        TemporaryRecording temporary(
            std::filesystem::temp_directory_path()
            / ("navalha-juce-recording-soak-"
               + std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count())
               + ".wav"));
        navalha::RecordingWriterService recorder(engine);
        if (!recorder.start(
                temporary.path, sampleRate, navalha::WavSampleFormat::pcm24,
                {"Recording soak", "Navalha 2", "JUCE migration", "", ""},
                targetFrames + sampleRate))
            throw std::runtime_error("Unable to start recording soak writer: "
                                     + recorder.error());

        constexpr std::size_t blockSize = 512;
        std::vector<float> left(blockSize);
        std::vector<float> right(blockSize);
        std::uint64_t produced = 0;
        while (produced < targetFrames)
        {
            const auto count = static_cast<std::size_t>(
                std::min<std::uint64_t>(blockSize, targetFrames - produced));
            engine.processBlock(left.data(), right.data(), count);
            produced += count;
            while (produced > recorder.framesWritten() + 32768)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (!engine.submitCommand({navalha::EngineCommandType::stopRecording}))
            throw std::runtime_error("Unable to stop recording soak capture");
        engine.processBlock(left.data(), right.data(), 1);
        const auto drainDeadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (recorder.framesWritten() < targetFrames
               && std::chrono::steady_clock::now() < drainDeadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        recorder.stop();

        if (!recorder.error().empty())
            throw std::runtime_error("Recording soak writer failed: "
                                     + recorder.error());
        if (recorder.framesWritten() != targetFrames)
            throw std::runtime_error("Recording soak frame count mismatch");
        if (engine.droppedRecordingFrames() != 0)
            throw std::runtime_error("Recording soak dropped realtime frames");

        const auto bytes = readBytes(temporary.path);
        const auto decoded = navalha::decodeWav(bytes, targetFrames);
        if (decoded->size() != targetFrames
            || decoded->sampleRate() != sampleRate)
            throw std::runtime_error("Published recording failed WAV validation");

        std::cout << "recording_frames=" << targetFrames
                  << " bytes=" << bytes.size()
                  << " drops=0 seconds=" << seconds << '\n';
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Recording soak failed: " << exception.what() << '\n';
        return 1;
    }
}
