#pragma once

#include <string>
#include <string_view>

#include "core/ProjectState.h"

namespace navalha
{
[[nodiscard]] std::string encodeProjectJson(const ProjectStateV2& project);
[[nodiscard]] ProjectStateV2 decodeProjectJson(std::string_view text);
}
