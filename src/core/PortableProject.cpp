#include "core/PortableProject.h"

#include <stdexcept>
#include <string>

#include "core/PortableArchive.h"
#include "core/ProjectJson.h"
#include "core/WavMemoryReader.h"

namespace navalha
{
std::vector<std::uint8_t> createPortableProject(
    const ProjectStateV2& project,
    std::span<const std::uint8_t> sourceAWav,
    std::span<const std::uint8_t> sourceBWav)
{
    const auto json = encodeProjectJson(project);
    std::vector<PortableEntry> entries {
        {"project.navalha", {json.begin(), json.end()}}
    };
    if (!sourceAWav.empty())
        entries.push_back({"audio/source-a.wav", {sourceAWav.begin(), sourceAWav.end()}});
    if (!sourceBWav.empty())
        entries.push_back({"audio/source-b.wav", {sourceBWav.begin(), sourceBWav.end()}});
    return encodePortableArchive(entries);
}

LoadedPortableProject openPortableProject(std::span<const std::uint8_t> archive)
{
    LoadedPortableProject loaded;
    bool hasProject = false;
    for (auto& entry : decodePortableArchive(archive))
    {
        if (entry.path == "project.navalha")
        {
            const std::string json(entry.data.begin(), entry.data.end());
            loaded.project = decodeProjectJson(json);
            hasProject = true;
        }
        else if (entry.path == "audio/source-a.wav")
        {
            loaded.sourceA = decodeWav(entry.data);
            loaded.sourceAWav = std::move(entry.data);
        }
        else if (entry.path == "audio/source-b.wav")
        {
            loaded.sourceB = decodeWav(entry.data);
            loaded.sourceBWav = std::move(entry.data);
        }
        else
        {
            throw std::invalid_argument("Portable project contains an unexpected file");
        }
    }
    if (!hasProject)
        throw std::invalid_argument("Portable project is missing project.navalha");
    return loaded;
}
}
