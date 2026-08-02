#pragma once

#include "core/AssistedRng.h"
#include "core/FormDirector.h"
#include "core/PatternTransform.h"

namespace navalha
{
struct AssistedPerformerSettings
{
    bool enabled = false;
    bool repeat = true;
    bool chooseSource = true;
    bool changeOrder = true;
    bool editRegion = true;
    bool editSlices = false;
    bool autoMix = false;
    bool applyTransform = true;
    bool useGaps = false;
    bool changePitch = true;
    bool useFragments = true;
    int minBpm = 72;
    int maxBpm = 144;
    int variation = 48;
};

enum class AssistedFragment {none, stutter, burst, reverse};
enum class AssistedCutAction {none, nudge, micro, blade, undo, redivide};
struct AssistedRegion { double start = 0.0; double end = 1.0; };
struct AssistedMixState
{
    double balance = 0.0;
    std::array<double, 2> pan {};
    std::array<double, 2> width {1.0, 1.0};
    bool available = false;
};

struct AssistedPhraseInput
{
    int currentBpm = 120;
    std::size_t currentSource = 0;
    std::size_t currentPattern = 0;
    std::array<bool, 2> playable {};
    std::array<std::size_t, 2> sliceCounts {8, 8};
    std::array<Pattern, patternCount> patterns {};
    std::array<PatternMemory, patternCount> memory {};
    std::array<AssistedRegion, 2> regions;
    AssistedMixState mixer;
};

struct AssistedPhraseDecision
{
    int bpm = 120;
    bool changesPitch = false;
    int pitch = 0;
    bool transforms = false;
    PatternTransformAmounts transform;
    AssistedFragment fragment = AssistedFragment::none;
    AssistedPerformanceContext context;
    bool switchesSource = false;
    std::size_t source = 0;
    bool selectsPattern = false;
    std::size_t pattern = 0;
    bool changesPattern = false;
    Pattern patternRow {};
    bool editsRegion = false;
    std::size_t regionSource = 0;
    AssistedRegion region;
    std::size_t regionDivision = 8;
    AssistedCutAction cutAction = AssistedCutAction::none;
    std::size_t cutSource = 0;
    std::size_t cutIndex = 0;
    double cutValue = 0.0;
    std::size_t cutCount = 0;
    bool mixes = false;
    AssistedMixState mixer;
};

void normalizeAssistedSettings(AssistedPerformerSettings& settings) noexcept;
[[nodiscard]] AssistedPhraseDecision planAssistedPhrase(
    const AssistedPerformerSettings& settings,
    const FormDirectorState& form,
    const AssistedPhraseInput& input,
    AssistedRng& random) noexcept;
}
