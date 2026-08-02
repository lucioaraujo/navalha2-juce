#include "core/RecordingWriterService.h"

#include <chrono>
#include <exception>

namespace navalha
{
RecordingWriterService::RecordingWriterService(AudioEngine& audioEngine) noexcept
    : engine(audioEngine)
{
}

RecordingWriterService::~RecordingWriterService()
{
    stop();
}

bool RecordingWriterService::start(const std::filesystem::path& path,
                                   std::uint32_t sampleRate,
                                   WavSampleFormat format,
                                   WavMetadata metadata,
                                   std::uint64_t maximumFrames)
{
    if (running.load(std::memory_order_acquire) || worker.joinable())
        return false;

    {
        const std::lock_guard lock(errorMutex);
        errorMessage.clear();
    }
    writtenFrames.store(0, std::memory_order_relaxed);
    frameLimit = maximumFrames;
    finalPath = path;
    partialPath = path.parent_path()
        / ("." + path.filename().string() + "."
           + std::to_string(
               std::chrono::steady_clock::now().time_since_epoch().count())
           + ".partial");
    output.open(partialPath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        const std::lock_guard lock(errorMutex);
        errorMessage = "Unable to open recording output";
        std::error_code ignoredError;
        std::filesystem::remove(partialPath, ignoredError);
        return false;
    }

    try
    {
        writer = std::make_unique<WavStreamWriter>(
            output, sampleRate, format, std::move(metadata));
    }
    catch (const std::exception& exception)
    {
        const std::lock_guard lock(errorMutex);
        errorMessage = exception.what();
        output.close();
        std::error_code ignoredError;
        std::filesystem::remove(partialPath, ignoredError);
        return false;
    }

    if (!engine.submitCommand({EngineCommandType::startRecording}))
    {
        const std::lock_guard lock(errorMutex);
        errorMessage = "Audio command queue is full";
        writer.reset();
        output.close();
        std::error_code ignoredError;
        std::filesystem::remove(partialPath, ignoredError);
        return false;
    }

    stopRequested.store(false, std::memory_order_release);
    running.store(true, std::memory_order_release);
    worker = std::thread([this] { run(); });
    return true;
}

void RecordingWriterService::stop()
{
    if (!worker.joinable())
        return;

    static_cast<void>(engine.submitCommand({EngineCommandType::stopRecording}));
    stopRequested.store(true, std::memory_order_release);
    worker.join();
    running.store(false, std::memory_order_release);
    writer.reset();
    output.close();

    std::string currentError;
    {
        const std::lock_guard lock(errorMutex);
        currentError = errorMessage;
    }
    if (currentError.empty())
    {
        std::error_code renameError;
        std::filesystem::rename(partialPath, finalPath, renameError);
        if (renameError)
        {
            const std::lock_guard lock(errorMutex);
            errorMessage = "Unable to publish finalized recording: "
                + renameError.message();
        }
    }
    if (!error().empty())
    {
        std::error_code ignoredError;
        std::filesystem::remove(partialPath, ignoredError);
    }
}

bool RecordingWriterService::isRunning() const noexcept
{
    return running.load(std::memory_order_acquire);
}

std::uint64_t RecordingWriterService::framesWritten() const noexcept
{
    return writtenFrames.load(std::memory_order_relaxed);
}

std::string RecordingWriterService::error() const
{
    const std::lock_guard lock(errorMutex);
    return errorMessage;
}

void RecordingWriterService::run() noexcept
{
    try
    {
        auto stopObservedAt = std::chrono::steady_clock::time_point {};
        for (;;)
        {
            StereoSample sample;
            bool consumed = false;
            while (engine.popRecordedFrame(sample))
            {
                if (frameLimit != 0
                    && writtenFrames.load(std::memory_order_relaxed) >= frameLimit)
                {
                    const std::lock_guard lock(errorMutex);
                    errorMessage = "Recording safety limit reached";
                    stopRequested.store(true, std::memory_order_release);
                    break;
                }
                writer->writeFrame(sample);
                writtenFrames.fetch_add(1, std::memory_order_relaxed);
                consumed = true;
            }

            if (stopRequested.load(std::memory_order_acquire))
            {
                if (stopObservedAt == std::chrono::steady_clock::time_point {})
                    stopObservedAt = std::chrono::steady_clock::now();
                if (!engine.isRecordingActive())
                    break;
                if (std::chrono::steady_clock::now() - stopObservedAt
                    >= std::chrono::milliseconds(250))
                {
                    const std::lock_guard lock(errorMutex);
                    if (errorMessage.empty())
                        errorMessage =
                            "Recording stop was not acknowledged by the audio callback";
                    break;
                }
            }
            if (!consumed)
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        writer->finalize();
    }
    catch (const std::exception& exception)
    {
        const std::lock_guard lock(errorMutex);
        errorMessage = exception.what();
    }
    running.store(false, std::memory_order_release);
}
}
