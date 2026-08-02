#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "core/AudioEngine.h"
#include "core/WavStreamWriter.h"

namespace navalha
{
class RecordingWriterService
{
public:
    explicit RecordingWriterService(AudioEngine& audioEngine) noexcept;
    ~RecordingWriterService();

    RecordingWriterService(const RecordingWriterService&) = delete;
    RecordingWriterService& operator=(const RecordingWriterService&) = delete;

    [[nodiscard]] bool start(const std::filesystem::path& path,
                             std::uint32_t sampleRate,
                             WavSampleFormat format,
                             WavMetadata metadata = {},
                             std::uint64_t maximumFrames = 0);
    void stop();

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] std::uint64_t framesWritten() const noexcept;
    [[nodiscard]] std::string error() const;

private:
    void run() noexcept;

    AudioEngine& engine;
    std::filesystem::path finalPath;
    std::filesystem::path partialPath;
    std::ofstream output;
    std::unique_ptr<WavStreamWriter> writer;
    std::thread worker;
    std::atomic<bool> stopRequested {false};
    std::atomic<bool> running {false};
    std::atomic<std::uint64_t> writtenFrames {0};
    std::uint64_t frameLimit = 0;
    mutable std::mutex errorMutex;
    std::string errorMessage;
};
}
