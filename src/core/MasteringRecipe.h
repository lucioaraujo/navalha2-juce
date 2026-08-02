#pragma once

#include <string>
#include <string_view>

#include "core/Json.h"
#include "core/MasteringProcessor.h"

namespace navalha
{
struct MasteringRecipe
{
    std::string source;
    std::string createdAt;
    MasteringParameters parameters;
};

[[nodiscard]] std::string encodeMasteringRecipe(const MasteringRecipe& recipe);
[[nodiscard]] MasteringRecipe decodeMasteringRecipe(std::string_view json);
[[nodiscard]] Json encodeMasteringParameters(
    const MasteringParameters& parameters);
[[nodiscard]] MasteringParameters decodeMasteringParameters(const Json& value);
}
