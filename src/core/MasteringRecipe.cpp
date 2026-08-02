#include "core/MasteringRecipe.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "core/Json.h"

namespace navalha
{
namespace
{
const Json* child(const Json* value, std::string_view key) noexcept
{
    return value == nullptr ? nullptr : value->find(key);
}

double requiredFiniteNumber(const Json* parent, std::string_view key)
{
    const auto* value = child(parent, key);
    if (value == nullptr)
        throw std::invalid_argument("MASTER recipe is missing parameter: "
                                    + std::string(key));
    const auto number = value->number(std::numeric_limits<double>::quiet_NaN());
    if (!std::isfinite(number))
        throw std::invalid_argument("MASTER recipe parameter is not finite: "
                                    + std::string(key));
    return number;
}

void validate(const MasteringParameters& parameters)
{
    if (parameters.trimDb < -12.0 || parameters.trimDb > 12.0)
        throw std::invalid_argument("MASTER recipe trim is outside +/-12 dB");
    MasteringProcessor processor;
    processor.prepare(48000.0, parameters);
}
}

std::string encodeMasteringRecipe(const MasteringRecipe& recipe)
{
    validate(recipe.parameters);
    return serializeJson(Json::Object {
        {"format", "navalha-master-recipe"},
        {"version", 1},
        {"appVersion", "0.28.1"},
        {"source", recipe.source},
        {"createdAt", recipe.createdAt},
        {"metering", "internal-estimate-not-EBU-certified"},
        {"parameters", encodeMasteringParameters(recipe.parameters)}
    }) + "\n";
}

MasteringRecipe decodeMasteringRecipe(std::string_view json)
{
    constexpr std::size_t maximumRecipeBytes = 1024 * 1024;
    const auto root = parseJson(json, maximumRecipeBytes, 16);
    if (!root.isObject()
        || child(&root, "format") == nullptr
        || child(&root, "format")->string() != "navalha-master-recipe"
        || child(&root, "version") == nullptr
        || child(&root, "version")->number() != 1.0)
        throw std::invalid_argument("Unsupported MASTER recipe format or version");
    const auto* parameters = child(&root, "parameters");
    if (parameters == nullptr || !parameters->isObject())
        throw std::invalid_argument("MASTER recipe has no parameters object");

    MasteringRecipe recipe;
    if (const auto* source = child(&root, "source"))
        recipe.source = std::string(source->string());
    if (const auto* createdAt = child(&root, "createdAt"))
        recipe.createdAt = std::string(createdAt->string());
    recipe.parameters = decodeMasteringParameters(*parameters);
    return recipe;
}

Json encodeMasteringParameters(const MasteringParameters& parameters)
{
    validate(parameters);
    return Json::Object {
        {"trim", parameters.trimDb},
        {"hpf", parameters.highPassHz},
        {"low", parameters.lowShelfDb},
        {"presence", parameters.presenceDb},
        {"high", parameters.highShelfDb},
        {"threshold", parameters.compressorThresholdDb},
        {"ratio", parameters.compressorRatio},
        {"width", parameters.width},
        {"saturation", parameters.saturation},
        {"ceiling", parameters.ceilingDb}
    };
}

MasteringParameters decodeMasteringParameters(const Json& value)
{
    if (!value.isObject())
        throw std::invalid_argument("MASTER parameters must be an object");
    MasteringParameters parameters {
        requiredFiniteNumber(&value, "trim"),
        requiredFiniteNumber(&value, "hpf"),
        requiredFiniteNumber(&value, "low"),
        requiredFiniteNumber(&value, "presence"),
        requiredFiniteNumber(&value, "high"),
        requiredFiniteNumber(&value, "threshold"),
        requiredFiniteNumber(&value, "ratio"),
        requiredFiniteNumber(&value, "width"),
        requiredFiniteNumber(&value, "saturation"),
        requiredFiniteNumber(&value, "ceiling")
    };
    validate(parameters);
    return parameters;
}
}
