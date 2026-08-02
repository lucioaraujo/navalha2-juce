#include "core/ControlTrace.h"

#include <algorithm>
#include <cmath>

namespace navalha
{
void ControlTrace::clear() noexcept { count = 0; }

bool ControlTrace::append(
    std::uint32_t timeMs, int bpm, int pitch, bool force) noexcept
{
    if (count == maxTracePoints)
        return false;
    const ControlTracePoint next {
        timeMs, std::clamp(bpm, 20, 400), std::clamp(pitch, -12, 11)};
    if (!force && count != 0)
    {
        const auto& previous = values[count - 1];
        if (next.timeMs < previous.timeMs + 38
            || (next.bpm == previous.bpm && next.pitch == previous.pitch))
            return false;
    }
    values[count++] = next;
    return true;
}

void ControlTrace::restore(std::span<const ControlTracePoint> points) noexcept
{
    count = std::min(points.size(), maxTracePoints);
    std::copy_n(points.begin(), count, values.begin());
    for (std::size_t index = 0; index < count; ++index)
    {
        values[index].bpm = std::clamp(values[index].bpm, 20, 400);
        values[index].pitch = std::clamp(values[index].pitch, -12, 11);
    }
    std::stable_sort(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(count),
        [] (const auto& left, const auto& right)
        {
            return left.timeMs < right.timeMs;
        });
}

std::span<const ControlTracePoint> ControlTrace::points() const noexcept
{
    return {values.data(), count};
}
std::size_t ControlTrace::size() const noexcept { return count; }
std::uint32_t ControlTrace::durationMs() const noexcept
{
    return count == 0 ? 0 : values[count - 1].timeMs;
}

void ControlTracePlayer::prepare(double sampleRate)
{
    framesPerMillisecond = std::max(1.0, sampleRate) / 1000.0;
    reset();
}
bool ControlTracePlayer::start(const ControlTrace& trace, bool loop) noexcept
{
    if (trace.size() < 2)
        return false;
    source = &trace;
    looping = loop;
    playing = true;
    frame = 0;
    cursor = 0;
    cycles = 0;
    cycleFrames = static_cast<std::uint64_t>(
        std::llround(std::max<std::uint32_t>(80, trace.durationMs())
                     * framesPerMillisecond))
        + static_cast<std::uint64_t>(std::llround(20.0 * framesPerMillisecond));
    return true;
}
void ControlTracePlayer::stop() noexcept
{
    playing = false;
    source = nullptr;
}
void ControlTracePlayer::reset() noexcept
{
    stop();
    frame = 0;
    cycleFrames = 0;
    cursor = 0;
    cycles = 0;
}
const ControlTracePoint* ControlTracePlayer::advance() noexcept
{
    if (!playing || source == nullptr)
        return nullptr;
    const auto points = source->points();
    const ControlTracePoint* due = nullptr;
    while (cursor < points.size())
    {
        const auto pointFrame = static_cast<std::uint64_t>(
            std::llround(points[cursor].timeMs * framesPerMillisecond));
        if (pointFrame > frame)
            break;
        due = &points[cursor++];
    }
    ++frame;
    if (frame >= cycleFrames)
    {
        if (looping)
        {
            frame = 0;
            cursor = 0;
            ++cycles;
        }
        else
            stop();
    }
    return due;
}
bool ControlTracePlayer::isPlaying() const noexcept { return playing; }
std::size_t ControlTracePlayer::cycleCount() const noexcept { return cycles; }
}
