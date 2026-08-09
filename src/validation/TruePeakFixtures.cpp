#include "validation/TruePeakFixtures.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace navalha::validation
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr std::size_t sampleRate = 48000;
constexpr std::size_t outputFrames = sampleRate;

std::vector<float> makeTaperedTone(double frequencyDivisor,
                                   double amplitude,
                                   double phaseDegrees)
{
    std::vector<float> result(outputFrames, 0.0F);
    for (std::size_t frame = 0; frame < outputFrames; ++frame)
    {
        const auto phase = 2.0 * pi
                * (static_cast<double>(sampleRate) / frequencyDivisor)
                * static_cast<double>(frame) / static_cast<double>(sampleRate)
            + phaseDegrees * pi / 180.0;
        auto envelope = 1.0;
        constexpr std::size_t fadeFrames = sampleRate / 100;
        if (frame < fadeFrames)
            envelope = static_cast<double>(frame) / fadeFrames;
        else if (frame + fadeFrames > outputFrames)
            envelope = static_cast<double>(outputFrames - frame - 1)
                / fadeFrames;
        result[frame] = static_cast<float>(
            std::sin(phase) * amplitude * envelope);
    }
    return result;
}

std::vector<float> makeTransient(std::size_t phaseOffset)
{
    constexpr std::size_t factor = 4;
    constexpr std::size_t highRateFrames = outputFrames * factor;
    constexpr std::size_t burstStart = highRateFrames / 2;
    constexpr std::size_t burstFrames = 16;
    constexpr std::size_t filterTaps = 511;
    constexpr auto filterCentre = filterTaps / 2;
    if (phaseOffset >= factor)
        throw std::invalid_argument("EBU transient phase must be in [0, 3]");

    std::vector<double> highRate(highRateFrames + filterTaps, 0.0);
    double phase = 0.0;
    for (std::size_t frame = 0; frame < highRateFrames; ++frame)
    {
        if (frame == burstStart)
            phase = 0.0;
        const auto inBurst = frame >= burstStart
            && frame < burstStart + burstFrames;
        const auto amplitude = inBurst ? 1.0 : 0.5;
        highRate[frame + filterCentre] = amplitude * std::sin(phase);
        phase += inBurst ? pi / 8.0 : pi / 12.0;
        if (frame + 1 == burstStart + burstFrames)
            phase = 0.0;
        if (phase >= 2.0 * pi)
            phase -= 2.0 * pi;
    }

    constexpr double cutoff = 0.12;
    std::array<double, filterTaps> coefficients {};
    double coefficientSum = 0.0;
    for (std::size_t tap = 0; tap < filterTaps; ++tap)
    {
        const auto position = static_cast<double>(tap)
            - static_cast<double>(filterCentre);
        const auto sinc = std::abs(position) < 1.0e-12
            ? 2.0 * cutoff
            : std::sin(2.0 * pi * cutoff * position) / (pi * position);
        const auto windowPhase = 2.0 * pi * static_cast<double>(tap)
            / static_cast<double>(filterTaps - 1);
        const auto window = 0.35875
            - 0.48829 * std::cos(windowPhase)
            + 0.14128 * std::cos(2.0 * windowPhase)
            - 0.01168 * std::cos(3.0 * windowPhase);
        coefficients[tap] = sinc * window;
        coefficientSum += coefficients[tap];
    }
    for (auto& coefficient : coefficients)
        coefficient /= coefficientSum;

    std::vector<float> result(outputFrames, 0.0F);
    for (std::size_t frame = 0; frame < outputFrames; ++frame)
    {
        const auto highRateFrame = frame * factor + phaseOffset;
        double filtered = 0.0;
        for (std::size_t tap = 0; tap < filterTaps; ++tap)
            filtered += coefficients[tap] * highRate[highRateFrame + tap];
        auto envelope = 1.0;
        constexpr std::size_t fadeFrames = sampleRate / 100;
        if (frame < fadeFrames)
            envelope = static_cast<double>(frame) / fadeFrames;
        else if (frame + fadeFrames > outputFrames)
            envelope = static_cast<double>(outputFrames - frame - 1)
                / fadeFrames;
        result[frame] = static_cast<float>(filtered * envelope);
    }
    return result;
}
}

std::vector<TruePeakFixture> makeEbuTruePeakFixtures()
{
    std::vector<TruePeakFixture> fixtures;
    fixtures.reserve(9);
    fixtures.push_back({15, -6.0, false, makeTaperedTone(4.0, 0.50, 0.0)});
    fixtures.push_back({16, -6.0, false, makeTaperedTone(4.0, 0.50, 45.0)});
    fixtures.push_back({17, -6.0, false, makeTaperedTone(6.0, 0.50, 60.0)});
    fixtures.push_back({18, -6.0, false, makeTaperedTone(8.0, 0.50, 67.5)});
    fixtures.push_back({19, 3.0, false, makeTaperedTone(4.0, 1.41, 45.0)});
    for (std::size_t phaseOffset = 0; phaseOffset < 4; ++phaseOffset)
        fixtures.push_back({
            static_cast<int>(20 + phaseOffset), 0.0, true,
            makeTransient(phaseOffset)});
    return fixtures;
}
}
