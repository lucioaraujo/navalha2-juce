#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace navalha
{
constexpr std::size_t patternCount = 10;
constexpr std::size_t stepsPerPattern = 8;
constexpr std::uint16_t gapCellCode = 256;

struct PatternCell
{
    enum class Kind
    {
        sourceA,
        sourceB,
        gap
    };

    Kind kind = Kind::sourceA;
    std::uint8_t sliceIndex = 0;

    [[nodiscard]] static PatternCell decode(std::uint16_t code);
    [[nodiscard]] std::uint16_t encode() const noexcept;
};

using Pattern = std::array<std::uint16_t, stepsPerPattern>;

class PatternBank
{
public:
    PatternBank() noexcept;

    void setPattern(std::size_t patternIndex, const Pattern& pattern);
    void setCell(std::size_t patternIndex, std::size_t stepIndex, std::uint16_t code);

    [[nodiscard]] const Pattern& pattern(std::size_t patternIndex) const;
    [[nodiscard]] std::uint16_t cell(std::size_t patternIndex, std::size_t stepIndex) const;

private:
    static void validateCode(std::uint16_t code);
    std::array<Pattern, patternCount> values {};
};

struct SequencerEvent
{
    std::size_t step = 0;
    PatternCell cell;
};

enum class TimingMode
{
    grid,
    free,
    jitter
};

class Sequencer
{
public:
    explicit Sequencer(const PatternBank& patterns) noexcept;

    void prepare(double newSampleRate);
    void setTempo(double newBpm, std::size_t newDivisionMode);
    void setTiming(TimingMode mode, double jitterPercent, std::uint32_t seed);
    void selectPattern(std::size_t patternIndex);
    void start() noexcept;
    void stop() noexcept;
    void reset() noexcept;

    // Call exactly once per rendered sample. This path does not allocate or lock.
    [[nodiscard]] std::optional<SequencerEvent> processSample() noexcept;

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] std::size_t currentStep() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] double tempo() const noexcept;
    [[nodiscard]] std::size_t division() const noexcept;
    [[nodiscard]] TimingMode timing() const noexcept;
    [[nodiscard]] double jitterPercent() const noexcept;
    [[nodiscard]] std::uint32_t seed() const noexcept;
    [[nodiscard]] std::size_t currentPattern() const noexcept;

private:
    void updateInterval() noexcept;
    void scheduleNextEvent() noexcept;
    [[nodiscard]] double randomUnit() noexcept;

    const PatternBank& bank;
    double sampleRate = 44100.0;
    double bpm = 120.0;
    std::size_t divisionMode = 0;
    std::size_t selectedPattern = 0;
    std::size_t step = 0;
    std::uint64_t samplePosition = 0;
    std::uint64_t transportGeneration = 0;
    long double nextEventSample = 0.0L;
    long double samplesPerStep = 22050.0L;
    TimingMode timingMode = TimingMode::grid;
    double jitter = 18.0;
    std::uint32_t timingSeed = 0x4e415632U;
    std::uint32_t randomState = timingSeed;
    bool running = false;
};
}
