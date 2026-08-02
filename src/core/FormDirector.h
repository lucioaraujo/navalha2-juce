#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

namespace navalha
{
constexpr std::size_t maxFormScenes = 16;
using FormText = std::array<char, 37>;

[[nodiscard]] FormText makeFormText(std::string_view value) noexcept;
[[nodiscard]] std::string_view formText(const FormText& value) noexcept;

enum class FormTransition
{
    cut, crossfade, dissolve, accumulate, erode, rupture, silence
};

enum class SliceBankProfile
{
    working, longSlices, medium, shortSlices, micro, manual, region
};

struct FormScene
{
    FormText key {};
    FormText name {};
    int bars = 4;
    int energy = 0;
    int variation = 0;
    FormTransition transition = FormTransition::cut;
    SliceBankProfile bankA = SliceBankProfile::working;
    SliceBankProfile bankB = SliceBankProfile::working;
    int density = 0;
    int tension = 0;
    int continuity = 0;
    int contrast = 0;
    int stability = 0;
    int stereoMotion = 0;
    bool locked = false;
};
static_assert(std::is_trivially_copyable_v<FormScene>);

struct FormDirectorState
{
    bool enabled = false;
    bool hold = false;
    std::size_t currentScene = 0;
    int bar = 0;
    bool completed = false;
    std::array<FormScene, maxFormScenes> scenes {};
    std::size_t sceneCount = 0;
};

struct AssistedPerformanceContext
{
    double intensity = 0.0;
    double energy = 0.0;
    bool formActive = false;
    std::size_t sceneIndex = 0;
};

[[nodiscard]] FormDirectorState defaultFormDirector();
void normalizeFormDirector(FormDirectorState& state);
[[nodiscard]] AssistedPerformanceContext assistedPerformanceContext(
    const FormDirectorState& state, double manualVariation) noexcept;
[[nodiscard]] std::size_t generatedSliceCount(
    SliceBankProfile profile) noexcept;

class FormDirector
{
public:
    FormDirector();

    [[nodiscard]] const FormDirectorState& state() const noexcept;
    void restore(FormDirectorState restored);
    void setEnabled(bool enabled) noexcept;
    void toggleHold() noexcept;
    void reset() noexcept;
    [[nodiscard]] bool selectScene(std::size_t index) noexcept;
    [[nodiscard]] bool advanceScene() noexcept;
    [[nodiscard]] bool notePhraseCompleted() noexcept;
    [[nodiscard]] bool addScene();
    [[nodiscard]] bool duplicateScene();
    [[nodiscard]] bool deleteScene() noexcept;
    [[nodiscard]] bool moveScene(int delta) noexcept;
    [[nodiscard]] bool replaceCurrentScene(FormScene scene);
    [[nodiscard]] bool toggleCurrentLock() noexcept;

private:
    FormDirectorState value;
};

[[nodiscard]] const char* toString(FormTransition value) noexcept;
[[nodiscard]] const char* toString(SliceBankProfile value) noexcept;
[[nodiscard]] FormTransition formTransitionFromString(
    const std::string& value, FormTransition fallback);
[[nodiscard]] SliceBankProfile sliceBankProfileFromString(
    const std::string& value, SliceBankProfile fallback);
}
