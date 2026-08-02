#include "core/AssistedPerformer.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace navalha
{
namespace
{
int randomInt(AssistedRng& random, int minimum, int maximum) noexcept
{
    const auto low = std::min(minimum, maximum);
    const auto high = std::max(minimum, maximum);
    return low + static_cast<int>(
        std::floor(random.next() * std::max(1, high - low + 1)));
}
}

void normalizeAssistedSettings(AssistedPerformerSettings& settings) noexcept
{
    settings.minBpm = std::clamp(settings.minBpm, 20, 400);
    settings.maxBpm = std::clamp(settings.maxBpm, 20, 400);
    if (settings.minBpm > settings.maxBpm)
        std::swap(settings.minBpm, settings.maxBpm);
    settings.variation = std::clamp(settings.variation, 0, 100);
}

AssistedPhraseDecision planAssistedPhrase(
    const AssistedPerformerSettings& rawSettings,
    const FormDirectorState& form,
    const AssistedPhraseInput& input,
    AssistedRng& random) noexcept
{
    auto settings = rawSettings;
    normalizeAssistedSettings(settings);
    AssistedPhraseDecision result;
    result.context = assistedPerformanceContext(form, settings.variation);
    const auto intensity = result.context.intensity;
    const auto energy = result.context.energy;
    result.source = input.currentSource;
    result.pattern = input.currentPattern;
    result.mixer = input.mixer;

    std::array<std::size_t, 2> playable {};
    std::size_t playableCount = 0;
    for (std::size_t source = 0; source < input.playable.size(); ++source)
        if (input.playable[source])
            playable[playableCount++] = source;
    if (settings.chooseSource && playableCount != 0)
    {
        auto next = input.currentSource;
        if (!input.playable[next])
            next = playable[0];
        else if (playableCount > 1
                 && random.next() < 0.18 + intensity * 0.42)
            next = next == 0 ? 1 : 0;
        result.switchesSource = next != input.currentSource;
        result.source = next;
    }
    if (settings.changeOrder
        && random.next() <= 0.12 + intensity * 0.38)
    {
        const auto roll = random.next();
        result.pattern = roll < 0.38 ? (input.currentPattern + 1) % 10
            : roll < 0.76 ? (input.currentPattern + 9) % 10
            : static_cast<std::size_t>(randomInt(random, 0, 9));
        if (result.pattern == input.currentPattern)
            result.pattern = (result.pattern + 1) % 10;
        result.selectsPattern = true;
    }
    result.patternRow = input.patterns[result.pattern];
    if (settings.changeOrder && playableCount != 0)
    {
        auto candidate = result.patternRow;
        const auto baseline = candidate;
        const auto& memory = input.memory[result.pattern];
        std::array<std::uint16_t, 256> pool {};
        std::size_t poolCount = 0;
        for (std::size_t source = 0; source < 2; ++source)
            if (input.playable[source])
                for (std::size_t slice = 0;
                     slice < input.sliceCounts[source] && poolCount < pool.size();
                     ++slice)
                    pool[poolCount++] = static_cast<std::uint16_t>(
                        slice + (source == 1 ? 128U : 0U));
        const auto style = random.next();
        if (intensity > 0.52 && style < 0.18)
            std::reverse(candidate.begin(), candidate.end());
        else if (playableCount > 1 && intensity > 0.35 && style < 0.40)
            for (std::size_t step = 0; step < candidate.size(); ++step)
            {
                const auto source = playable[step % playableCount];
                candidate[step] = static_cast<std::uint16_t>(
                    randomInt(random, 0,
                        static_cast<int>(input.sliceCounts[source] - 1))
                    + (source == 1 ? 128 : 0));
            }
        else
        {
            const auto ceiling = std::max(
                1, static_cast<int>(std::lround(1.0 + intensity * 7.0)));
            const auto changes = randomInt(random, 1, ceiling);
            for (int change = 0; change < changes; ++change)
            {
                const auto step = static_cast<std::size_t>(
                    randomInt(random, 0, 7));
                if (memory[step])
                    continue;
                const auto gapChance = settings.useGaps && intensity > 0.62
                    ? (intensity - 0.62) * 0.22 : 0.0;
                candidate[step] = random.next() < gapChance
                    ? gapCellCode
                    : pool[static_cast<std::size_t>(
                        randomInt(random, 0,
                                  static_cast<int>(poolCount - 1)))];
            }
        }
        for (std::size_t step = 0; step < candidate.size(); ++step)
            if (memory[step])
                candidate[step] = baseline[step];
        if (settings.useGaps && poolCount != 0)
        {
            const auto minimumSounding = result.context.formActive ? 2 : 4;
            auto sounding = static_cast<int>(std::count_if(
                candidate.begin(), candidate.end(),
                [] (auto cell) { return cell != gapCellCode; }));
            for (std::size_t step = 0;
                 step < candidate.size() && sounding < minimumSounding; ++step)
                if (!memory[step] && candidate[step] == gapCellCode)
                {
                    candidate[step] = pool[static_cast<std::size_t>(
                        randomInt(random, 0,
                                  static_cast<int>(poolCount - 1)))];
                    ++sounding;
                }
        }
        result.changesPattern = candidate != baseline;
        result.patternRow = candidate;
    }
    if (settings.editRegion && playableCount != 0
        && random.next() <= 0.08 + intensity * 0.30)
    {
        result.editsRegion = true;
        result.regionSource = input.playable[input.currentSource]
            ? input.currentSource
            : playable[static_cast<std::size_t>(
                randomInt(random, 0, static_cast<int>(playableCount - 1)))];
        const auto previous = input.regions[result.regionSource];
        if (random.next() < 0.20)
            result.region = {0.0, 1.0};
        else
        {
            const auto width = std::max(0.04, previous.end - previous.start);
            const auto targetWidth = std::clamp(
                width * (0.42 + random.next() * 0.72), 0.04, 1.0);
            const auto anchor = std::clamp(
                (previous.start + previous.end) * 0.5
                    + (random.next() * 2.0 - 1.0) * width * 0.18,
                0.0, 1.0);
            const auto start = std::clamp(
                anchor - targetWidth * 0.5, 0.0, 1.0 - targetWidth);
            result.region = {start, start + targetWidth};
        }
        constexpr std::array<std::size_t, 5> choices {4, 8, 16, 32, 64};
        const auto current = input.sliceCounts[result.regionSource];
        std::array<std::size_t, 5> available {};
        std::size_t count = 0;
        for (const auto choice : choices)
            if (choice >= std::min<std::size_t>(current, 64))
                available[count++] = choice;
        const auto reach = std::max<std::size_t>(1, std::min(
            count, 1 + static_cast<std::size_t>(
                std::lround(intensity * (count - 1)))));
        result.regionDivision = available[static_cast<std::size_t>(
            randomInt(random, 0, static_cast<int>(reach - 1)))];
    }
    if (settings.editSlices && playableCount != 0
        && random.next() <= 0.14 + intensity * 0.40)
    {
        result.cutSource = playable[static_cast<std::size_t>(
            randomInt(random, 0, static_cast<int>(playableCount - 1)))];
        const auto count = std::max<std::size_t>(
            1, input.sliceCounts[result.cutSource]);
        result.cutIndex = static_cast<std::size_t>(
            randomInt(random, 0, static_cast<int>(count - 1)));
        const auto roll = random.next();
        result.cutAction = roll < 0.38 ? AssistedCutAction::nudge
            : roll < 0.62 ? AssistedCutAction::micro
            : roll < 0.80 ? AssistedCutAction::blade
            : roll < 0.90 ? AssistedCutAction::undo
            : intensity >= 0.58 ? AssistedCutAction::redivide
            : AssistedCutAction::nudge;
        result.cutValue = random.next();
        result.cutCount = static_cast<std::size_t>(
            randomInt(random, 2, intensity > 0.72 ? 4 : 3));
    }

    const auto current = std::clamp(
        input.currentBpm, settings.minBpm, settings.maxBpm);
    if (settings.minBpm == settings.maxBpm)
        result.bpm = settings.minBpm;
    else if (result.context.formActive)
    {
        const auto range = settings.maxBpm - settings.minBpm;
        const auto target = static_cast<int>(std::lround(
            settings.minBpm + range * energy));
        const auto reach = std::max(1, static_cast<int>(std::lround(
            range * (0.04 + intensity * 0.22))));
        const auto drift = static_cast<int>(std::lround(
            (target - current) * (0.30 + intensity * 0.30)));
        const auto scatter = std::max(1, static_cast<int>(std::lround(
            reach * (0.10 + intensity * 0.24))));
        result.bpm = std::clamp(
            current + std::clamp(
                drift + randomInt(random, -scatter, scatter), -reach, reach),
            settings.minBpm, settings.maxBpm);
        if (result.bpm == current && current != target)
            result.bpm += target > current ? 1 : -1;
    }
    else
    {
        const auto range = settings.maxBpm - settings.minBpm;
        const auto reach = std::max(1, static_cast<int>(std::lround(
            range * (0.08 + intensity * 0.30))));
        result.bpm = std::clamp(
            current + randomInt(random, -reach, reach),
            settings.minBpm, settings.maxBpm);
        if (result.bpm == current)
            result.bpm = std::clamp(
                current + (random.next() < 0.5 ? -1 : 1),
                settings.minBpm, settings.maxBpm);
    }

    if (settings.applyTransform && intensity >= 0.20
        && random.next() <= 0.06 + intensity * 0.34)
    {
        const auto amount = std::clamp(static_cast<int>(std::lround(
            8.0 + random.next() * (18.0 + intensity * 66.0))), 8, 92);
        const auto roll = random.next();
        result.transforms = true;
        if (!settings.useGaps || roll < 0.38)
            result.transform.mutation = amount;
        else if (roll < 0.66)
            result.transform.erosion = amount;
        else if (roll < 0.90 || intensity < 0.72)
            result.transform.deconstruct = amount;
        else
            result.transform = {
                static_cast<int>(std::lround(amount * 0.55)),
                static_cast<int>(std::lround(amount * 0.28)),
                static_cast<int>(std::lround(amount * 0.72))};
    }

    if (settings.changePitch && intensity >= 0.48
        && random.next() <= 0.08 + (intensity - 0.48) * 0.32)
    {
        constexpr std::array candidates {-12, -7, -5, -3, 0, 3, 5, 7, 11};
        const auto span = std::clamp(
            static_cast<int>(std::lround(2.0 + intensity * 9.0)), 2, 11);
        std::array<int, candidates.size()> playable {};
        std::size_t count = 0;
        for (const auto candidate : candidates)
            if (std::abs(candidate) <= span)
                playable[count++] = candidate;
        result.changesPitch = true;
        result.pitch = playable[static_cast<std::size_t>(
            randomInt(random, 0, static_cast<int>(count - 1)))];
    }

    if (settings.useFragments && intensity >= 0.20
        && random.next()
            <= 0.03 + intensity * 0.14 + energy * 0.18)
    {
        const auto roll = random.next();
        result.fragment = roll < 0.55 ? AssistedFragment::stutter
            : roll < 0.82 && intensity > 0.62 ? AssistedFragment::burst
            : AssistedFragment::reverse;
    }
    if (settings.autoMix && input.mixer.available
        && random.next() <= 0.08 + intensity * 0.22)
    {
        result.mixes = true;
        const auto balanceLimit = std::clamp(
            0.30 + energy * 0.22, 0.30, 0.52);
        result.mixer.balance = std::clamp(
            input.mixer.balance
                + (random.next() - 0.5) * (0.12 + intensity * 0.18),
            -balanceLimit, balanceLimit);
        const auto panLimit = std::clamp(
            0.28 + energy * 0.20, 0.28, 0.48);
        for (std::size_t source = 0; source < 2; ++source)
        {
            const auto direction = source == 0 ? -1.0 : 1.0;
            const auto home = direction * (0.06 + energy * 0.12);
            const auto drift =
                (random.next() - 0.5) * (0.08 + intensity * 0.14);
            result.mixer.pan[source] = std::clamp(
                input.mixer.pan[source] * 0.72 + home * 0.28 + drift,
                -panLimit, panLimit);
        }
        if (random.next() < 0.22 + intensity * 0.18)
            for (std::size_t source = 0; source < 2; ++source)
                result.mixer.width[source] = std::clamp(
                    input.mixer.width[source]
                        + (random.next() - 0.5)
                            * (0.08 + intensity * 0.12),
                    0.85, 1.25);
    }
    return result;
}
}
