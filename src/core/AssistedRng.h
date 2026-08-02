#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace navalha
{
class AssistedRng
{
public:
    static constexpr std::uint32_t defaultSeed = 0x4e415632U;

    explicit AssistedRng(std::uint32_t seed = defaultSeed) noexcept;

    void rewind() noexcept;
    void setSeed(std::uint32_t seed) noexcept;
    void restore(std::uint32_t seed,
                 std::uint32_t state,
                 std::uint64_t cursor) noexcept;
    [[nodiscard]] double next() noexcept;

    [[nodiscard]] std::uint32_t seed() const noexcept;
    [[nodiscard]] std::uint32_t state() const noexcept;
    [[nodiscard]] std::uint64_t cursor() const noexcept;

    [[nodiscard]] static std::string formatSeed(std::uint32_t seed);
    [[nodiscard]] static bool parseSeed(std::string_view text, std::uint32_t& result) noexcept;

private:
    std::uint32_t initialSeed = defaultSeed;
    std::uint32_t currentState = defaultSeed;
    std::uint64_t decisionCursor = 0;
};
}
