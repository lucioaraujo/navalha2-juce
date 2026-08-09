#include "core/FormDirector.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace navalha
{
FormText makeFormText(std::string_view value) noexcept
{
    FormText result {};
    const auto count = std::min<std::size_t>(value.size(), result.size() - 1);
    std::copy_n(value.begin(), count, result.begin());
    return result;
}

std::string_view formText(const FormText& value) noexcept
{
    const auto end = std::find(value.begin(), value.end(), '\0');
    return {value.data(), static_cast<std::size_t>(
        std::distance(value.begin(), end))};
}

namespace
{
FormScene scene(std::string_view key, std::string_view name, int bars, int energy,
                int variation, FormTransition transition, SliceBankProfile bankA,
                SliceBankProfile bankB, int density, int tension, int continuity,
                int contrast, int stability, int stereoMotion)
{
    return {makeFormText(key), makeFormText(name), bars, energy, variation, transition,
            bankA, bankB, density, tension, continuity, contrast, stability,
            stereoMotion, false};
}

std::string upper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

void normalizeScene(FormScene& value, std::size_t index)
{
    const auto defaults = defaultFormDirector();
    const auto& fallback = defaults.scenes[
        std::min(index, defaults.sceneCount - 1)];
    if (formText(value.key).empty())
        value.key = fallback.key;
    if (formText(value.name).empty())
        value.name = fallback.name;
    value.bars = std::clamp(value.bars, 1, 128);
    for (auto* amount : {&value.energy, &value.variation, &value.density,
                         &value.tension, &value.continuity, &value.contrast,
                         &value.stability, &value.stereoMotion})
        *amount = std::clamp(*amount, 0, 100);
}

}

FormDirectorState defaultFormDirector()
{
    FormDirectorState state;
    state.sceneCount = 5;
    state.scenes[0] = scene("intro", "INTRO", 4, 22, 18,
        FormTransition::cut, SliceBankProfile::longSlices,
        SliceBankProfile::longSlices, 24, 18, 82, 24, 78, 12);
    state.scenes[1] = scene("development", "DEVELOPMENT", 8, 48, 42,
        FormTransition::crossfade, SliceBankProfile::medium,
        SliceBankProfile::medium, 48, 44, 68, 42, 58, 32);
    state.scenes[2] = scene("rupture", "RUPTURE", 4, 72, 86,
        FormTransition::rupture, SliceBankProfile::shortSlices,
        SliceBankProfile::shortSlices, 66, 86, 24, 88, 18, 62);
    state.scenes[3] = scene("climax", "CLIMAX", 8, 92, 68,
        FormTransition::accumulate, SliceBankProfile::micro,
        SliceBankProfile::shortSlices, 92, 78, 48, 64, 28, 78);
    state.scenes[4] = scene("exit", "EXIT", 4, 28, 24,
        FormTransition::dissolve, SliceBankProfile::longSlices,
        SliceBankProfile::longSlices, 22, 18, 74, 30, 72, 18);
    return state;
}

void normalizeFormDirector(FormDirectorState& state)
{
    if (state.sceneCount == 0)
        state = defaultFormDirector();
    state.sceneCount = std::clamp<std::size_t>(
        state.sceneCount, 1, maxFormScenes);
    for (std::size_t index = 0; index < state.sceneCount; ++index)
        normalizeScene(state.scenes[index], index);
    state.currentScene = std::min(state.currentScene, state.sceneCount - 1);
    state.bar = std::clamp(state.bar, 0, state.scenes[state.currentScene].bars);
    if (!state.enabled)
        state.hold = false;
}

AssistedPerformanceContext assistedPerformanceContext(
    const FormDirectorState& state, double manualVariation) noexcept
{
    const auto manual = std::clamp(manualVariation, 0.0, 100.0) / 100.0;
    if (!state.enabled || state.sceneCount == 0)
        return {manual, manual, false, 0};
    const auto index = std::min(state.currentScene, state.sceneCount - 1);
    const auto& scene = state.scenes[index];
    const auto density = scene.density / 100.0;
    const auto tension = scene.tension / 100.0;
    const auto stability = scene.stability / 100.0;
    return {
        std::clamp(
            (manual + scene.variation / 100.0 + tension + (1.0 - stability))
                / 4.0,
            0.0, 1.0),
        std::clamp((scene.energy / 100.0 + density) / 2.0, 0.0, 1.0),
        true, index
    };
}

std::size_t generatedSliceCount(SliceBankProfile profile) noexcept
{
    switch (profile)
    {
        case SliceBankProfile::longSlices: return 4;
        case SliceBankProfile::medium: return 8;
        case SliceBankProfile::shortSlices: return 16;
        case SliceBankProfile::micro: return 32;
        default: return 0;
    }
}

FormDirector::FormDirector()
    : value(defaultFormDirector()), history(std::make_unique<EditHistory>())
{
}
const FormDirectorState& FormDirector::state() const noexcept { return value; }
void FormDirector::restore(FormDirectorState restored)
{
    normalizeFormDirector(restored);
    value = std::move(restored);
    history->undoCount = 0;
    history->redoCount = 0;
}
void FormDirector::setEnabled(bool enabled) noexcept
{
    value.enabled = enabled;
    if (!enabled)
        value.hold = false;
}
void FormDirector::toggleHold() noexcept
{
    if (value.enabled)
    {
        value.hold = !value.hold;
        value.completed = false;
    }
}
void FormDirector::reset() noexcept
{
    value.currentScene = 0;
    value.bar = 0;
    value.completed = false;
    value.hold = false;
}
bool FormDirector::selectScene(std::size_t index) noexcept
{
    if (index >= value.sceneCount)
        return false;
    value.currentScene = index;
    value.bar = 0;
    value.completed = false;
    return true;
}
bool FormDirector::advanceScene() noexcept
{
    if (value.currentScene + 1 < value.sceneCount)
        return selectScene(value.currentScene + 1);
    value.bar = value.scenes[value.currentScene].bars;
    value.completed = true;
    value.hold = true;
    return false;
}
bool FormDirector::notePhraseCompleted() noexcept
{
    if (!value.enabled || value.hold || value.completed)
        return false;
    const auto bars = value.scenes[value.currentScene].bars;
    value.bar = std::min(bars, value.bar + 1);
    return value.bar >= bars ? advanceScene() : true;
}
bool FormDirector::addScene()
{
    if (value.sceneCount >= maxFormScenes)
        return false;
    pushUndo();
    auto copy = value.scenes[value.currentScene];
    char text[37] {};
    static_cast<void>(std::snprintf(
        text, sizeof(text), "scene%zu", value.sceneCount + 1));
    copy.key = makeFormText(text);
    static_cast<void>(std::snprintf(
        text, sizeof(text), "SCENE %zu", value.sceneCount + 1));
    copy.name = makeFormText(text);
    copy.locked = false;
    const auto insert = value.currentScene + 1;
    std::move_backward(value.scenes.begin() + static_cast<std::ptrdiff_t>(insert),
        value.scenes.begin() + static_cast<std::ptrdiff_t>(value.sceneCount),
        value.scenes.begin() + static_cast<std::ptrdiff_t>(value.sceneCount + 1));
    value.scenes[insert] = std::move(copy);
    ++value.sceneCount;
    return selectScene(insert);
}
bool FormDirector::duplicateScene()
{
    if (value.sceneCount >= maxFormScenes)
        return false;
    pushUndo();
    auto copy = value.scenes[value.currentScene];
    auto name = formText(copy.name);
    FormText copiedName {};
    const auto baseCount = std::min<std::size_t>(name.size(), 36);
    std::copy_n(name.begin(), baseCount, copiedName.begin());
    constexpr std::string_view suffix {" COPY"};
    const auto suffixCount = std::min(
        suffix.size(), copiedName.size() - 1 - baseCount);
    std::copy_n(suffix.begin(), suffixCount, copiedName.begin() + baseCount);
    copy.name = copiedName;
    copy.locked = false;
    const auto insert = value.currentScene + 1;
    std::move_backward(value.scenes.begin() + static_cast<std::ptrdiff_t>(insert),
        value.scenes.begin() + static_cast<std::ptrdiff_t>(value.sceneCount),
        value.scenes.begin() + static_cast<std::ptrdiff_t>(value.sceneCount + 1));
    value.scenes[insert] = std::move(copy);
    ++value.sceneCount;
    return selectScene(insert);
}
bool FormDirector::deleteScene() noexcept
{
    if (value.sceneCount <= 1)
        return false;
    pushUndo();
    std::move(value.scenes.begin() + static_cast<std::ptrdiff_t>(value.currentScene + 1),
        value.scenes.begin() + static_cast<std::ptrdiff_t>(value.sceneCount),
        value.scenes.begin() + static_cast<std::ptrdiff_t>(value.currentScene));
    --value.sceneCount;
    value.currentScene = std::min(value.currentScene, value.sceneCount - 1);
    value.bar = 0;
    value.completed = false;
    return true;
}
bool FormDirector::moveScene(int delta) noexcept
{
    const auto target = std::clamp(
        static_cast<int>(value.currentScene) + delta, 0,
        static_cast<int>(value.sceneCount - 1));
    if (target == static_cast<int>(value.currentScene))
        return false;
    pushUndo();
    std::swap(value.scenes[value.currentScene],
              value.scenes[static_cast<std::size_t>(target)]);
    value.currentScene = static_cast<std::size_t>(target);
    return true;
}
bool FormDirector::replaceCurrentScene(FormScene scene, bool recordHistory)
{
    if (value.scenes[value.currentScene].locked)
        return false;
    if (recordHistory)
        pushUndo();
    normalizeScene(scene, value.currentScene);
    value.scenes[value.currentScene] = std::move(scene);
    value.bar = std::min(value.bar, value.scenes[value.currentScene].bars);
    value.completed = false;
    return true;
}
bool FormDirector::toggleCurrentLock() noexcept
{
    pushUndo();
    auto& locked = value.scenes[value.currentScene].locked;
    locked = !locked;
    return locked;
}

void FormDirector::checkpointEdit() noexcept
{
    pushUndo();
}

void FormDirector::pushSnapshot(
    std::array<EditSnapshot, maxFormHistory>& history,
    std::size_t& count, EditSnapshot snapshot) noexcept
{
    if (count == maxFormHistory)
    {
        for (std::size_t index = 1; index < count; ++index)
            history[index - 1] = std::move(history[index]);
        --count;
    }
    history[count++] = std::move(snapshot);
}

FormDirector::EditSnapshot FormDirector::editSnapshot() const noexcept
{
    return {value.scenes, value.sceneCount};
}

void FormDirector::restoreEditSnapshot(EditSnapshot snapshot) noexcept
{
    value.scenes = std::move(snapshot.scenes);
    value.sceneCount = std::clamp<std::size_t>(
        snapshot.sceneCount, 1, maxFormScenes);
    value.currentScene = std::min(value.currentScene, value.sceneCount - 1);
    value.bar = std::clamp(
        value.bar, 0, value.scenes[value.currentScene].bars);
    value.completed = false;
}

void FormDirector::pushUndo() noexcept
{
    pushSnapshot(history->undo, history->undoCount, editSnapshot());
    history->redoCount = 0;
}

bool FormDirector::undoEdit() noexcept
{
    if (history->undoCount == 0)
        return false;
    pushSnapshot(history->redo, history->redoCount, editSnapshot());
    restoreEditSnapshot(std::move(history->undo[--history->undoCount]));
    return true;
}

bool FormDirector::redoEdit() noexcept
{
    if (history->redoCount == 0)
        return false;
    pushSnapshot(history->undo, history->undoCount, editSnapshot());
    restoreEditSnapshot(std::move(history->redo[--history->redoCount]));
    return true;
}

bool FormDirector::canUndo() const noexcept { return history->undoCount != 0; }
bool FormDirector::canRedo() const noexcept { return history->redoCount != 0; }

const char* toString(FormTransition value) noexcept
{
    constexpr const char* names[] {
        "CUT", "CROSSFADE", "DISSOLVE", "ACCUMULATE", "ERODE", "RUPTURE", "SILENCE"};
    return names[static_cast<std::size_t>(value)];
}
const char* toString(SliceBankProfile value) noexcept
{
    constexpr const char* names[] {
        "WORKING", "LONG", "MEDIUM", "SHORT", "MICRO", "MANUAL", "REGION"};
    return names[static_cast<std::size_t>(value)];
}
FormTransition formTransitionFromString(
    const std::string& value, FormTransition fallback)
{
    const auto normalized = upper(value);
    for (std::size_t index = 0; index < 7; ++index)
    {
        const auto candidate = static_cast<FormTransition>(index);
        if (normalized == toString(candidate))
            return candidate;
    }
    return fallback;
}
SliceBankProfile sliceBankProfileFromString(
    const std::string& value, SliceBankProfile fallback)
{
    const auto normalized = upper(value);
    for (std::size_t index = 0; index < 7; ++index)
    {
        const auto candidate = static_cast<SliceBankProfile>(index);
        if (normalized == toString(candidate))
            return candidate;
    }
    return fallback;
}
}
