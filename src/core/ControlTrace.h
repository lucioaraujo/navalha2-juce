#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace navalha
{
constexpr std::size_t maxTracePoints = 512;

struct ControlTracePoint
{
    std::uint32_t timeMs = 0;
    int bpm = 120;
    int pitch = 0;

    bool operator==(const ControlTracePoint&) const = default;
};

class ControlTrace
{
public:
    void clear() noexcept;
    [[nodiscard]] bool append(
        std::uint32_t timeMs, int bpm, int pitch, bool force = false) noexcept;
    void restore(std::span<const ControlTracePoint> points) noexcept;
    [[nodiscard]] std::span<const ControlTracePoint> points() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::uint32_t durationMs() const noexcept;

private:
    std::array<ControlTracePoint, maxTracePoints> values {};
    std::size_t count = 0;
};

class ControlTracePlayer
{
public:
    void prepare(double sampleRate);
    [[nodiscard]] bool start(
        const ControlTrace& trace, bool loop = true) noexcept;
    void stop() noexcept;
    void reset() noexcept;
    [[nodiscard]] const ControlTracePoint* advance() noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] std::size_t cycleCount() const noexcept;

private:
    const ControlTrace* source = nullptr;
    double framesPerMillisecond = 44.1;
    std::uint64_t frame = 0;
    std::uint64_t cycleFrames = 0;
    std::size_t cursor = 0;
    std::size_t cycles = 0;
    bool looping = true;
    bool playing = false;
};
}
