#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "core/StereoMixer.h"

namespace navalha
{
template <std::size_t Capacity>
class RecordingFifo
{
    static_assert(Capacity >= 2);

public:
    [[nodiscard]] bool push(StereoSample sample) noexcept
    {
        const auto write = writeIndex.load(std::memory_order_relaxed);
        const auto next = increment(write);
        if (next == readIndex.load(std::memory_order_acquire))
        {
            droppedFrames.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        frames[write] = sample;
        writeIndex.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool pop(StereoSample& sample) noexcept
    {
        const auto read = readIndex.load(std::memory_order_relaxed);
        if (read == writeIndex.load(std::memory_order_acquire))
            return false;
        sample = frames[read];
        readIndex.store(increment(read), std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::uint64_t dropped() const noexcept
    {
        return droppedFrames.load(std::memory_order_relaxed);
    }

    void resetDropped() noexcept
    {
        droppedFrames.store(0, std::memory_order_relaxed);
    }

private:
    [[nodiscard]] static constexpr std::size_t increment(std::size_t index) noexcept
    {
        return (index + 1) % Capacity;
    }

    std::array<StereoSample, Capacity> frames {};
    alignas(64) std::atomic<std::size_t> writeIndex {0};
    alignas(64) std::atomic<std::size_t> readIndex {0};
    std::atomic<std::uint64_t> droppedFrames {0};
};
}
