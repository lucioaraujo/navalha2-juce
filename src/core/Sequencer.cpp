#include "core/Sequencer.h"

#include <algorithm>
#include <cmath>

namespace navalha
{
PatternCell PatternCell::decode(std::uint16_t code)
{
    if (code <= 127)
        return {Kind::sourceA, static_cast<std::uint8_t>(code)};
    if (code <= 255)
        return {Kind::sourceB, static_cast<std::uint8_t>(code - 128)};
    if (code == gapCellCode)
        return {Kind::gap, 0};

    throw std::out_of_range("Pattern cell must be between 0 and 256");
}

std::uint16_t PatternCell::encode() const noexcept
{
    if (kind == Kind::gap)
        return gapCellCode;

    return static_cast<std::uint16_t>(sliceIndex)
        + (kind == Kind::sourceB ? 128U : 0U);
}

PatternBank::PatternBank() noexcept
{
    for (auto& row : values)
    {
        for (std::size_t stepIndex = 0; stepIndex < row.size(); ++stepIndex)
            row[stepIndex] = static_cast<std::uint16_t>(stepIndex);
    }
}

void PatternBank::validateCode(std::uint16_t code)
{
    if (code > gapCellCode)
        throw std::out_of_range("Pattern cell must be between 0 and 256");
}

void PatternBank::setPattern(std::size_t patternIndex, const Pattern& patternValue)
{
    if (patternIndex >= values.size())
        throw std::out_of_range("Pattern index must be between 0 and 9");
    for (const auto code : patternValue)
        validateCode(code);

    values[patternIndex] = patternValue;
}

void PatternBank::setCell(std::size_t patternIndex, std::size_t stepIndex, std::uint16_t code)
{
    if (patternIndex >= values.size())
        throw std::out_of_range("Pattern index must be between 0 and 9");
    if (stepIndex >= stepsPerPattern)
        throw std::out_of_range("Step index must be between 0 and 7");
    validateCode(code);
    values[patternIndex][stepIndex] = code;
}

const Pattern& PatternBank::pattern(std::size_t patternIndex) const
{
    if (patternIndex >= values.size())
        throw std::out_of_range("Pattern index must be between 0 and 9");
    return values[patternIndex];
}

std::uint16_t PatternBank::cell(std::size_t patternIndex, std::size_t stepIndex) const
{
    if (stepIndex >= stepsPerPattern)
        throw std::out_of_range("Step index must be between 0 and 7");
    return pattern(patternIndex)[stepIndex];
}

Sequencer::Sequencer(const PatternBank& patterns) noexcept
    : bank(patterns)
{
    updateInterval();
}

void Sequencer::prepare(double newSampleRate)
{
    if (!std::isfinite(newSampleRate) || newSampleRate <= 0.0)
        throw std::invalid_argument("Sample rate must be finite and positive");
    sampleRate = newSampleRate;
    updateInterval();
    reset();
}

void Sequencer::setTempo(double newBpm, std::size_t newDivisionMode)
{
    if (!std::isfinite(newBpm) || newBpm < 20.0 || newBpm > 400.0)
        throw std::out_of_range("Tempo must be between 20 and 400 BPM");
    if (newDivisionMode > 3)
        throw std::out_of_range("Division mode must be between 0 and 3");

    bpm = newBpm;
    divisionMode = newDivisionMode;
    updateInterval();
}

void Sequencer::setTiming(TimingMode mode, double jitterPercent, std::uint32_t seed)
{
    if (!std::isfinite(jitterPercent) || jitterPercent < 0.0 || jitterPercent > 40.0)
        throw std::out_of_range("Jitter must be between 0 and 40 percent");

    timingMode = mode;
    jitter = jitterPercent;
    timingSeed = seed == 0 ? 0x4e415632U : seed;
    randomState = timingSeed;
}

void Sequencer::selectPattern(std::size_t patternIndex)
{
    if (patternIndex >= patternCount)
        throw std::out_of_range("Pattern index must be between 0 and 9");
    selectedPattern = patternIndex;
}

void Sequencer::start() noexcept
{
    running = true;
    nextEventSample = static_cast<long double>(samplePosition);
}

void Sequencer::stop() noexcept
{
    running = false;
    ++transportGeneration;
}

void Sequencer::reset() noexcept
{
    step = 0;
    samplePosition = 0;
    nextEventSample = 0.0L;
    randomState = timingSeed;
    ++transportGeneration;
}

std::optional<SequencerEvent> Sequencer::processSample() noexcept
{
    std::optional<SequencerEvent> event;

    if (running && static_cast<long double>(samplePosition) >= nextEventSample)
    {
        event = SequencerEvent {step, PatternCell::decode(bank.cell(selectedPattern, step))};
        step = (step + 1) % stepsPerPattern;
        scheduleNextEvent();
    }

    ++samplePosition;
    return event;
}

bool Sequencer::isRunning() const noexcept
{
    return running;
}

std::size_t Sequencer::currentStep() const noexcept
{
    return step;
}

std::uint64_t Sequencer::generation() const noexcept
{
    return transportGeneration;
}

double Sequencer::tempo() const noexcept { return bpm; }
std::size_t Sequencer::division() const noexcept { return divisionMode; }
TimingMode Sequencer::timing() const noexcept { return timingMode; }
double Sequencer::jitterPercent() const noexcept { return jitter; }
std::uint32_t Sequencer::seed() const noexcept { return timingSeed; }
std::size_t Sequencer::currentPattern() const noexcept { return selectedPattern; }

void Sequencer::updateInterval() noexcept
{
    samplesPerStep = static_cast<long double>(sampleRate) * 60.0L
        / (static_cast<long double>(bpm) * static_cast<long double>(divisionMode + 1));
}

void Sequencer::scheduleNextEvent() noexcept
{
    long double factor = 1.0L;
    if (timingMode == TimingMode::jitter)
    {
        const auto spread = jitter / 100.0;
        factor = 1.0L + static_cast<long double>((randomUnit() * 2.0 - 1.0) * spread);
    }
    else if (timingMode == TimingMode::free)
    {
        factor = 0.48L + static_cast<long double>(randomUnit() * 1.08);
    }

    const auto minimumSamples = static_cast<long double>(sampleRate) * 0.018L;
    nextEventSample += std::max(minimumSamples, samplesPerStep * factor);
}

double Sequencer::randomUnit() noexcept
{
    auto value = randomState;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    randomState = value;
    return static_cast<double>(value) / 4294967296.0;
}
}
