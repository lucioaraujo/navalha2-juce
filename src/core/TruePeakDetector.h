#pragma once

#include <array>
#include <cstddef>

namespace navalha
{
// Four-times polyphase FIR detector intended for BS.1770/EBU true-peak
// validation. It has no allocation or locks in processSample(). Compliance is
// established by the EBU Tech 3341 cases in the contract tests, not by the
// class name alone.
class TruePeakDetector
{
public:
    static constexpr std::size_t oversamplingFactor = 4;
    static constexpr std::size_t tapsPerPhase = 16;

    void prepare(double sampleRate);
    void reset() noexcept;
    [[nodiscard]] float processSample(float input) noexcept;
    [[nodiscard]] std::size_t latencySamples() const noexcept;

private:
    std::array<std::array<float, tapsPerPhase>, oversamplingFactor> coefficients {};
    std::array<float, tapsPerPhase> history {};
    std::size_t writeIndex = 0;
};
}
