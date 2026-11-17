#pragma once

#include <array>
#include <cstddef>

namespace navalha
{
// Four-times polyphase FIR detector intended for BS.1770/EBU true-peak
// validation. It has no allocation or locks in processSample(). Compliance is
// established by the EBU Tech 3341 cases in the contract tests, not by the
// class name alone.
//
// 2026-11-17 (RASGO saida-ref 1.2, same author): 48 taps per phase, Kaiser
// (beta 9), phase 0 exact. The 16-tap Blackman-Harris filter lost 0.7 dB at
// 17 kHz and 2.7 dB at 19 kHz (44.1 kHz), so bright material read up to 1 dB
// under its real true peak; this one is flat (< 0.01 dB) to 19 kHz and
// -0.1 dB at 20 kHz. Latency 24 samples (was 8), inside the limiter's 5 ms.
class TruePeakDetector
{
public:
    static constexpr std::size_t oversamplingFactor = 4;
    static constexpr std::size_t tapsPerPhase = 48;  // a multiple of 8 (the partial sums)
    static_assert(tapsPerPhase % 8 == 0);

    void prepare(double sampleRate);
    void reset() noexcept;
    [[nodiscard]] float processSample(float input) noexcept;
    [[nodiscard]] std::size_t latencySamples() const noexcept;

private:
    // the three in-between phases, oldest-first; phase 0 is the input itself,
    // tapsPerPhase / 2 samples late
    std::array<std::array<float, tapsPerPhase>, oversamplingFactor - 1> coefficients {};
    // the history twice over: the newest tapsPerPhase samples are contiguous
    std::array<float, 2 * tapsPerPhase> history {};
    std::size_t writeIndex = 0;
};
}
