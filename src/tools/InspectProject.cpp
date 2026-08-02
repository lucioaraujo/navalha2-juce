#include "core/Json.h"
#include "core/ProjectJson.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argumentCount, char** arguments)
{
    try
    {
        if (argumentCount != 2)
        {
            std::cerr << "usage: navalha_inspect_project PROJECT.json\n";
            return 2;
        }

        constexpr std::uintmax_t maximumProjectBytes = 4ULL * 1024ULL * 1024ULL;
        const std::filesystem::path path(arguments[1]);
        const auto size = std::filesystem::file_size(path);
        if (size > maximumProjectBytes)
            throw std::length_error("Project exceeds the 4 MiB safety limit");

        std::string text(static_cast<std::size_t>(size), '\0');
        std::ifstream input(path, std::ios::binary);
        if (!input.read(text.data(), static_cast<std::streamsize>(text.size())))
            throw std::runtime_error("Unable to read project");

        const auto root = navalha::parseJson(text);
        const auto* versionValue = root.find("version");
        const auto inputVersion = versionValue == nullptr
            ? 0 : static_cast<int>(versionValue->number());
        const auto project = navalha::decodeProjectJson(text);
        const auto canonical = navalha::encodeProjectJson(project);
        const auto canonicalRoundTrip = navalha::encodeProjectJson(
            navalha::decodeProjectJson(canonical));
        if (canonicalRoundTrip != canonical)
            throw std::runtime_error("Canonical Project v2 round-trip is not idempotent");
        const auto soundCells = [&project]
        {
            std::size_t count = 0;
            for (std::size_t pattern = 0; pattern < navalha::patternCount; ++pattern)
                for (std::size_t step = 0; step < navalha::stepsPerPattern; ++step)
                    if (project.patterns.cell(pattern, step) != navalha::gapCellCode)
                        ++count;
            return count;
        }();

        std::cout << navalha::serializeJson(navalha::Json::Object {
            {"valid", true},
            {"input_version", inputVersion},
            {"canonical_version", 2},
            {"canonical_bytes", static_cast<double>(canonical.size())},
            {"canonical_idempotent", true},
            {"active_source", project.activeSource == 0 ? "A" : "B"},
            {"bpm", project.bpm},
            {"slices_a", static_cast<double>(project.sources[0].sliceBank.size())},
            {"slices_b", static_cast<double>(project.sources[1].sliceBank.size())},
            {"sound_cells", static_cast<double>(soundCells)},
            {"trace_points", static_cast<double>(project.controlTrace.size())},
            {"form_scenes", static_cast<double>(project.formDirector.sceneCount)}
        }) << '\n';
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Project inspection failed: " << exception.what() << '\n';
        return 1;
    }
}
