#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "core/ProjectState.h"
#include "core/SlicePlayer.h"

namespace navalha
{
struct LoadedPortableProject
{
    ProjectStateV2 project;
    std::unique_ptr<StereoAudioBuffer> sourceA;
    std::unique_ptr<StereoAudioBuffer> sourceB;
    std::vector<std::uint8_t> sourceAWav;
    std::vector<std::uint8_t> sourceBWav;
};

[[nodiscard]] std::vector<std::uint8_t> createPortableProject(
    const ProjectStateV2& project,
    std::span<const std::uint8_t> sourceAWav = {},
    std::span<const std::uint8_t> sourceBWav = {});

[[nodiscard]] LoadedPortableProject openPortableProject(
    std::span<const std::uint8_t> archive);
}
