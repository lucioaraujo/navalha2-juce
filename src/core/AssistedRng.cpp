#include "core/AssistedRng.h"

#include <charconv>
#include <cstdio>

namespace navalha
{
AssistedRng::AssistedRng(std::uint32_t seed) noexcept
    : initialSeed(seed), currentState(seed)
{
}

void AssistedRng::rewind() noexcept
{
    currentState = initialSeed;
    decisionCursor = 0;
}

void AssistedRng::setSeed(std::uint32_t seed) noexcept
{
    initialSeed = seed;
    rewind();
}

void AssistedRng::restore(std::uint32_t seed,
                          std::uint32_t state,
                          std::uint64_t cursor) noexcept
{
    initialSeed = seed;
    currentState = state;
    decisionCursor = cursor;
}

double AssistedRng::next() noexcept
{
    constexpr std::uint32_t step = 0x6d2b79f5U;
    currentState += step;
    auto mixed = currentState;
    mixed = (mixed ^ (mixed >> 15U)) * (mixed | 1U);
    mixed ^= mixed + (mixed ^ (mixed >> 7U)) * (mixed | 61U);
    const auto output = mixed ^ (mixed >> 14U);
    ++decisionCursor;
    return static_cast<double>(output) / 4294967296.0;
}

std::uint32_t AssistedRng::seed() const noexcept { return initialSeed; }
std::uint32_t AssistedRng::state() const noexcept { return currentState; }
std::uint64_t AssistedRng::cursor() const noexcept { return decisionCursor; }

std::string AssistedRng::formatSeed(std::uint32_t seed)
{
    char text[9] {};
    std::snprintf(text, sizeof(text), "%08X", seed);
    return text;
}

bool AssistedRng::parseSeed(std::string_view text, std::uint32_t& result) noexcept
{
    if (text.starts_with("0x") || text.starts_with("0X"))
        text.remove_prefix(2);
    if (text.empty() || text.size() > 8)
        return false;

    std::uint32_t parsed = 0;
    const auto conversion = std::from_chars(
        text.data(), text.data() + text.size(), parsed, 16);
    if (conversion.ec != std::errc {} || conversion.ptr != text.data() + text.size())
        return false;
    result = parsed;
    return true;
}
}
