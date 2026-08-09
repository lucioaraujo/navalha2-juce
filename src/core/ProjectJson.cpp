#include "core/ProjectJson.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "core/Json.h"

namespace navalha
{
namespace
{
Json encodeSlices(const SliceBank& bank)
{
    Json::Array values;
    for (const auto& slice : bank.slices())
        values.emplace_back(Json::Array {slice.start, slice.end});
    return Json(std::move(values));
}

Json encodeSource(const SourceState& source, const SourceReference& reference,
                  const NamedSliceBankStore& formBanks)
{
    Json::Object namedBanks;
    for (std::size_t index = 1; index < sliceBankProfileCount; ++index)
    {
        const auto profile = static_cast<SliceBankProfile>(index);
        if (formBanks.has(profile))
            namedBanks.emplace(
                toString(profile), encodeSlices(formBanks.bank(profile)));
    }
    return Json::Object {
        {"sample", Json::Object {
            {"filename", reference.filename},
            {"relativePath", reference.relativePath},
            {"size", static_cast<double>(reference.size)},
            {"lastModified", static_cast<double>(reference.lastModified)},
            {"type", reference.mediaType},
            {"embedded", false}
        }},
        {"slicing", Json::Object {
            {"slices", encodeSlices(source.sliceBank)},
            {"sliceBanks", std::move(namedBanks)},
            {"activeSliceBank", toString(formBanks.active())},
            {"storedCount", static_cast<double>(source.sliceBank.size())},
            {"operationalCount", static_cast<double>(source.sliceBank.size())}
        }}
    };
}

Json encodeMixerChannel(const MixerChannel& channel)
{
    return Json::Object {
        {"level", channel.level}, {"pan", channel.pan}, {"width", channel.width},
        {"mute", channel.muted}, {"solo", channel.solo}
    };
}

Json encodeVoice(const VirtualVoiceState& voice)
{
    Json::Array pattern;
    for (std::size_t index = 0; index < voice.patternLength; ++index)
        pattern.emplace_back(static_cast<double>(voice.pattern[index]));
    return Json::Object {
        {"enabled", voice.enabled},
        {"source", voice.sourceIndex == 1 ? "B" : "A"},
        {"division", static_cast<double>(voice.division)},
        {"pattern", std::move(pattern)},
        {"focusStart", voice.focusStart * 100.0},
        {"focusEnd", voice.focusEnd * 100.0},
        {"pitch", voice.pitchSemitones},
        {"level", voice.level}, {"pan", voice.pan},
        {"attack", voice.attackSeconds * 1000.0},
        {"release", voice.releaseSeconds * 1000.0}
    };
}

const Json* child(const Json* parent, std::string_view key)
{
    return parent == nullptr ? nullptr : parent->find(key);
}

double finiteNumber(const Json* value, double fallback)
{
    const auto number = value == nullptr ? fallback : value->number(fallback);
    return std::isfinite(number) ? number : fallback;
}

bool decodeSliceArray(const Json* slices, SliceBank& bank)
{
    if (slices == nullptr || !slices->isArray() || slices->array().empty())
        return false;

    std::array<Slice, maxSlices> decoded {};
    std::size_t count = 0;
    for (const auto& value : slices->array())
    {
        if (count == maxSlices || !value.isArray() || value.array().size() < 2)
            break;
        const Slice slice {
            finiteNumber(&value.array()[0], 0.0),
            finiteNumber(&value.array()[1], 1.0)
        };
        if (slice.isValid())
            decoded[count++] = slice;
    }
    if (count == 0)
        return false;
    bank.divideRegion(decoded[0].start, decoded[count - 1].end, count);
    for (std::size_t index = 0; index < count; ++index)
        bank.setSlice(index, decoded[index]);
    return true;
}

void decodeSlices(const Json* slicing, SliceBank& bank)
{
    static_cast<void>(decodeSliceArray(child(slicing, "slices"), bank));
}

void decodeNamedSliceBanks(
    const Json* slicing, NamedSliceBankStore& formBanks)
{
    const auto* namedBanks = child(slicing, "sliceBanks");
    for (std::size_t index = 1; index < sliceBankProfileCount; ++index)
    {
        const auto profile = static_cast<SliceBankProfile>(index);
        SliceBank decoded;
        if (decodeSliceArray(child(namedBanks, toString(profile)), decoded))
            formBanks.set(profile, decoded);
    }
    const auto active = child(slicing, "activeSliceBank");
    formBanks.setActive(active == nullptr
        ? SliceBankProfile::working
        : sliceBankProfileFromString(
            std::string(active->string()), SliceBankProfile::working));
}

MixerChannel decodeMixerChannel(const Json* value)
{
    MixerChannel channel;
    channel.level = finiteNumber(child(value, "level"), 1.0);
    channel.pan = finiteNumber(child(value, "pan"), 0.0);
    channel.width = finiteNumber(child(value, "width"), 1.0);
    channel.muted = child(value, "mute") != nullptr && child(value, "mute")->boolean();
    channel.solo = child(value, "solo") != nullptr && child(value, "solo")->boolean();
    return channel;
}

VirtualVoiceState decodeVoice(const Json& value)
{
    VirtualVoiceState voice;
    voice.enabled = child(&value, "enabled") != nullptr && child(&value, "enabled")->boolean();
    voice.sourceIndex = child(&value, "source") != nullptr
        && child(&value, "source")->string() == "B" ? 1 : 0;
    const auto division = static_cast<std::size_t>(
        finiteNumber(child(&value, "division"), 2.0));
    voice.division = division == 1 || division == 2 || division == 4 || division == 8
        ? division : 2;
    if (const auto* pattern = child(&value, "pattern"); pattern != nullptr && pattern->isArray())
    {
        voice.patternLength = std::clamp<std::size_t>(pattern->array().size(), 1, 16);
        for (std::size_t index = 0; index < voice.patternLength; ++index)
            voice.pattern[index] = static_cast<std::uint8_t>(std::clamp(
                finiteNumber(&pattern->array()[index], 0.0), 0.0, 127.0));
    }
    voice.focusStart = std::clamp(
        finiteNumber(child(&value, "focusStart"), 0.0) / 100.0, 0.0, 0.99);
    voice.focusEnd = std::clamp(
        finiteNumber(child(&value, "focusEnd"), 100.0) / 100.0,
        voice.focusStart + 0.01, 1.0);
    voice.pitchSemitones = static_cast<int>(std::clamp(
        finiteNumber(child(&value, "pitch"), 0.0), -12.0, 11.0));
    voice.level = std::clamp(finiteNumber(child(&value, "level"), 0.28), 0.0, 0.8);
    voice.pan = std::clamp(finiteNumber(child(&value, "pan"), 0.0), -1.0, 1.0);
    voice.attackSeconds = std::clamp(
        finiteNumber(child(&value, "attack"), 8.0) / 1000.0, 0.001, 0.5);
    voice.releaseSeconds = std::clamp(
        finiteNumber(child(&value, "release"), 80.0) / 1000.0, 0.005, 1.5);
    return voice;
}

Json encodeFormDirector(const FormDirectorState& form)
{
    Json::Array scenes;
    for (std::size_t index = 0; index < form.sceneCount; ++index)
    {
        const auto& scene = form.scenes[index];
        scenes.emplace_back(Json::Object {
            {"key", std::string(formText(scene.key))},
            {"name", std::string(formText(scene.name))}, {"bars", scene.bars},
            {"energy", scene.energy}, {"variation", scene.variation},
            {"transition", toString(scene.transition)},
            {"bankA", toString(scene.bankA)}, {"bankB", toString(scene.bankB)},
            {"density", scene.density}, {"tension", scene.tension},
            {"continuity", scene.continuity}, {"contrast", scene.contrast},
            {"stability", scene.stability}, {"stereoMotion", scene.stereoMotion},
            {"locked", scene.locked}
        });
    }
    return Json::Object {
        {"enabled", form.enabled}, {"hold", form.hold},
        {"currentScene", static_cast<double>(form.currentScene)},
        {"bar", form.bar}, {"completed", form.completed},
        {"scenes", std::move(scenes)}
    };
}

Json encodeControlTrace(const ControlTrace& trace)
{
    Json::Array points;
    for (const auto& point : trace.points())
        points.emplace_back(Json::Object {
            {"t", static_cast<double>(point.timeMs)},
            {"bpm", point.bpm}, {"pitch", point.pitch}
        });
    return Json::Object {{"points", std::move(points)}};
}

Json encodeMotifLocks(const MotifLocks& locks)
{
    return Json::Object {
        {"source", locks.source},
        {"cuts", locks.cuts},
        {"pattern", locks.pattern},
        {"transform", locks.transform},
        {"pitch", locks.pitch},
        {"gap", locks.gap},
        {"mix", locks.mix},
        {"voices", locks.voices}
    };
}

MotifLocks decodeMotifLocks(const Json* value)
{
    const auto enabled = [value] (std::string_view key)
    {
        const auto* item = child(value, key);
        return item != nullptr && item->boolean();
    };
    return {
        enabled("source"),
        enabled("cuts"),
        enabled("pattern"),
        enabled("transform"),
        enabled("pitch"),
        enabled("gap"),
        enabled("mix"),
        enabled("voices")
    };
}

Json encodeMotifSource(const SourceState& source)
{
    Json::Array cuts;
    const auto slices = source.sliceBank.slices();
    for (std::size_t index = 1; index < slices.size(); ++index)
        cuts.emplace_back(slices[index].start);
    const auto regionStart = slices.empty() ? 0.0 : slices.front().start;
    const auto regionEnd = slices.empty() ? 1.0 : slices.back().end;
    return Json::Object {
        {"region", Json::Array {regionStart, regionEnd}},
        {"slices", encodeSlices(source.sliceBank)},
        {"sliceCuts", std::move(cuts)},
        {"randomLimit", static_cast<double>(
            std::max<std::size_t>(1, source.sliceBank.size()))},
        {"currentSlice", 0},
        {"selection", Json::Array {
            slices.empty() ? 0.0 : slices.front().start,
            slices.empty() ? 1.0 : slices.front().end}},
        {"editMode", "region"}
    };
}

Json encodeMotif(const MotifSnapshot& motif)
{
    if (!motif.occupied)
        return Json(nullptr);
    Json::Array pattern;
    Json::Array memory;
    Json::Array voices;
    for (const auto cell : motif.pattern)
        pattern.emplace_back(static_cast<double>(cell));
    for (const auto protectedCell : motif.cellMemory)
        memory.emplace_back(protectedCell);
    for (const auto& voice : motif.virtualVoices)
        voices.push_back(encodeVoice(voice));
    const auto timingName = motif.timingMode == TimingMode::free ? "FREE"
        : motif.timingMode == TimingMode::jitter ? "JITTER" : "GRID";
    return Json::Object {
        {"name", motif.name},
        {"capturedAt", motif.capturedAt},
        {"activeSource", motif.activeSource == 1 ? "B" : "A"},
        {"currentPattern", static_cast<double>(motif.currentPattern)},
        {"pattern", std::move(pattern)},
        {"cellMemory", std::move(memory)},
        {"sources", Json::Object {
            {"A", encodeMotifSource(motif.sources[0])},
            {"B", encodeMotifSource(motif.sources[1])}
        }},
        {"bpm", motif.bpm},
        {"tmode", static_cast<double>(motif.divisionMode)},
        {"timing", Json::Object {
            {"mode", timingName}, {"jitter", motif.jitter}
        }},
        {"pitch", motif.heritagePitchSemitones},
        {"pitchMode", motif.heritagePitchMode},
        {"mixer", Json::Object {
            {"A", encodeMixerChannel(motif.mixer.sourceA)},
            {"B", encodeMixerChannel(motif.mixer.sourceB)},
            {"balance", motif.mixer.balance},
            {"autoEnabled", false}
        }},
        {"voices", std::move(voices)}
    };
}

MotifSnapshot decodeMotif(const Json& value, std::size_t slotIndex)
{
    MotifSnapshot motif;
    if (!value.isObject())
        return motif;
    motif.occupied = true;
    motif.name = child(&value, "name") == nullptr
        ? "MOTIF " + std::to_string(slotIndex + 1)
        : std::string(child(&value, "name")->string());
    if (motif.name.size() > 28)
        motif.name.resize(28);
    if (const auto* capturedAt = child(&value, "capturedAt"))
        motif.capturedAt = capturedAt->string();
    motif.activeSource = child(&value, "activeSource") != nullptr
        && child(&value, "activeSource")->string() == "B" ? 1 : 0;
    motif.currentPattern = static_cast<std::size_t>(std::clamp(
        finiteNumber(child(&value, "currentPattern"), 0.0), 0.0, 9.0));
    if (const auto* pattern = child(&value, "pattern");
        pattern != nullptr && pattern->isArray())
        for (std::size_t step = 0; step < stepsPerPattern; ++step)
            motif.pattern[step] = step < pattern->array().size()
                ? static_cast<std::uint16_t>(std::clamp(
                    finiteNumber(&pattern->array()[step], 0.0),
                    0.0, 256.0))
                : 0;
    if (const auto* memory = child(&value, "cellMemory");
        memory != nullptr && memory->isArray())
        for (std::size_t step = 0;
             step < std::min(memory->array().size(), stepsPerPattern); ++step)
            motif.cellMemory[step] = memory->array()[step].boolean();
    const auto* sources = child(&value, "sources");
    decodeSliceArray(
        child(child(sources, "A"), "slices"),
        motif.sources[0].sliceBank);
    decodeSliceArray(
        child(child(sources, "B"), "slices"),
        motif.sources[1].sliceBank);
    motif.bpm = std::clamp(
        finiteNumber(child(&value, "bpm"), 120.0), 20.0, 400.0);
    motif.divisionMode = static_cast<std::size_t>(std::clamp(
        finiteNumber(child(&value, "tmode"), 0.0), 0.0, 3.0));
    const auto* timing = child(&value, "timing");
    const auto timingMode = child(timing, "mode") == nullptr
        ? std::string_view("GRID") : child(timing, "mode")->string();
    motif.timingMode = timingMode == "FREE" ? TimingMode::free
        : timingMode == "JITTER" ? TimingMode::jitter : TimingMode::grid;
    motif.jitter = std::clamp(
        finiteNumber(child(timing, "jitter"), 18.0), 0.0, 40.0);
    motif.heritagePitchSemitones = static_cast<int>(std::clamp(
        finiteNumber(child(&value, "pitch"), 0.0), -12.0, 11.0));
    motif.heritagePitchMode = std::clamp(
        finiteNumber(child(&value, "pitchMode"), 0.0), 0.0, 1.0);
    const auto* mixer = child(&value, "mixer");
    motif.mixer.sourceA = decodeMixerChannel(child(mixer, "A"));
    motif.mixer.sourceB = decodeMixerChannel(child(mixer, "B"));
    motif.mixer.balance = finiteNumber(child(mixer, "balance"), 0.0);
    motif.mixer.normalize();
    if (const auto* voices = child(&value, "voices");
        voices != nullptr && voices->isArray())
        for (std::size_t index = 0;
             index < std::min<std::size_t>(
                 motif.virtualVoices.size(), voices->array().size()); ++index)
            motif.virtualVoices[index] = decodeVoice(voices->array()[index]);
    return motif;
}

FormDirectorState decodeFormDirector(const Json* value)
{
    auto result = defaultFormDirector();
    const auto* scenes = child(value, "scenes");
    if (scenes != nullptr && scenes->isArray() && !scenes->array().empty())
    {
        result.sceneCount = std::min<std::size_t>(
            scenes->array().size(), maxFormScenes);
        for (std::size_t index = 0; index < result.sceneCount; ++index)
        {
            const auto& source = scenes->array()[index];
            auto& target = result.scenes[index];
            if (const auto* item = child(&source, "key"))
                target.key = makeFormText(item->string(formText(target.key)));
            if (const auto* item = child(&source, "name"))
                target.name = makeFormText(item->string(formText(target.name)));
            target.bars = static_cast<int>(
                finiteNumber(child(&source, "bars"), target.bars));
            target.energy = static_cast<int>(
                finiteNumber(child(&source, "energy"), target.energy));
            target.variation = static_cast<int>(
                finiteNumber(child(&source, "variation"), target.variation));
            target.transition = formTransitionFromString(
                std::string(child(&source, "transition") == nullptr
                    ? std::string_view {} : child(&source, "transition")->string()),
                target.transition);
            target.bankA = sliceBankProfileFromString(
                std::string(child(&source, "bankA") == nullptr
                    ? std::string_view {} : child(&source, "bankA")->string()),
                target.bankA);
            target.bankB = sliceBankProfileFromString(
                std::string(child(&source, "bankB") == nullptr
                    ? std::string_view {} : child(&source, "bankB")->string()),
                target.bankB);
            for (const auto& field : {
                    std::pair {"density", &target.density},
                    std::pair {"tension", &target.tension},
                    std::pair {"continuity", &target.continuity},
                    std::pair {"contrast", &target.contrast},
                    std::pair {"stability", &target.stability},
                    std::pair {"stereoMotion", &target.stereoMotion}})
                *field.second = static_cast<int>(
                    finiteNumber(child(&source, field.first), *field.second));
            target.locked = child(&source, "locked") != nullptr
                && child(&source, "locked")->boolean();
        }
    }
    result.currentScene = static_cast<std::size_t>(std::max(
        0.0, finiteNumber(child(value, "currentScene"), 0.0)));
    result.bar = static_cast<int>(std::max(
        0.0, finiteNumber(child(value, "bar"), 0.0)));
    result.completed = child(value, "completed") != nullptr
        && child(value, "completed")->boolean();
    // Match the web app's safe project restore: FORM never resumes armed/held.
    result.enabled = false;
    result.hold = false;
    normalizeFormDirector(result);
    return result;
}
}

std::string encodeProjectJson(const ProjectStateV2& project)
{
    Json::Array patterns;
    for (std::size_t row = 0; row < patternCount; ++row)
    {
        Json::Array cells;
        for (const auto cell : project.patterns.pattern(row))
            cells.emplace_back(static_cast<double>(cell));
        patterns.emplace_back(std::move(cells));
    }

    Json::Array voices;
    for (const auto& voice : project.virtualVoices)
        voices.push_back(encodeVoice(voice));

    Json::Array memoryRows;
    for (const auto& memory : project.patternMemory)
    {
        Json::Array row;
        for (const auto protectedCell : memory)
            row.emplace_back(protectedCell);
        memoryRows.emplace_back(std::move(row));
    }
    Json transformBase(nullptr);
    if (project.patternTransform.hasBase)
    {
        Json::Array base;
        for (const auto cell : project.patternTransform.base)
            base.emplace_back(static_cast<double>(cell));
        transformBase = Json(std::move(base));
    }
    Json::Array motifSlots;
    for (const auto& motif : project.motifSlots)
        motifSlots.push_back(encodeMotif(motif));

    Json albumProject(nullptr);
    if (project.hasAlbumProject)
        albumProject = parseJson(encodeAlbumProject(project.albumProject));

    const auto timingName = project.timingMode == TimingMode::free ? "FREE"
        : project.timingMode == TimingMode::jitter ? "JITTER" : "GRID";
    Json root(Json::Object {
        {"format", "navalha-project"}, {"version", 2}, {"appVersion", "0.28.1"},
        {"activeSource", project.activeSource == 1 ? "B" : "A"},
        {"sources", Json::Object {
            {"A", encodeSource(project.sources[0], project.sourceReferences[0],
                                project.formSliceBanks[0])},
            {"B", encodeSource(project.sources[1], project.sourceReferences[1],
                                project.formSliceBanks[1])}
        }},
        {"sequencer", Json::Object {
            {"patterns", std::move(patterns)},
            {"currentPattern", static_cast<double>(project.currentPattern + 1)},
            {"bpm", project.bpm}, {"tmode", static_cast<double>(project.divisionMode)}
        }},
        {"dsp", Json::Object {
            {"pitch", project.heritagePitchSemitones},
            {"pitchMode", project.heritagePitchMode},
            {"gain", project.masterLevel},
            {"virtualVoices", std::move(voices)},
            {"sourceMixer", Json::Object {
                {"A", encodeMixerChannel(project.mixer.sourceA)},
                {"B", encodeMixerChannel(project.mixer.sourceB)},
                {"balance", project.mixer.balance},
                {"automation", Json::Object {
                    {"enabled", project.assisted.autoMix},
                    {"dimensions", Json::Array {"balance", "pan", "width"}},
                    {"excludes", Json::Array {"level", "mute", "solo"}}
                }}
            }}
        }},
        {"gesture", Json::Object {
            {"memory", std::move(memoryRows)},
            {"transform", Json::Object {
                {"patternIndex",
                 static_cast<double>(project.patternTransform.patternIndex)},
                {"base", std::move(transformBase)},
                {"mutationAmount",
                 project.patternTransform.amounts.mutation},
                {"erosionAmount",
                 project.patternTransform.amounts.erosion},
                {"deconstructAmount",
                 project.patternTransform.amounts.deconstruct}
            }},
            {"timing", Json::Object {
                {"mode", timingName}, {"jitter", project.jitter},
                {"seed", static_cast<double>(project.timingSeed)}
            }},
            {"assistedPerformer", Json::Object {
                {"enabled", project.assisted.enabled},
                {"repeat", project.assisted.repeat},
                {"chooseSource", project.assisted.chooseSource},
                {"changeOrder", project.assisted.changeOrder},
                {"editRegion", project.assisted.editRegion},
                {"editSlices", project.assisted.editSlices},
                {"autoMix", project.assisted.autoMix},
                {"applyTransform", project.assisted.applyTransform},
                {"useGaps", project.assisted.useGaps},
                {"changePitch", project.assisted.changePitch},
                {"useFragments", project.assisted.useFragments},
                {"minBpm", project.assisted.minBpm},
                {"maxBpm", project.assisted.maxBpm},
                {"variation", project.assisted.variation},
                {"decisionSeed", AssistedRng::formatSeed(project.assistedSeed)},
                {"decisionState", static_cast<double>(project.assistedState)},
                {"decisionCursor", static_cast<double>(project.assistedCursor)}
            }},
            {"motifMemory", Json::Object {
                {"selected", static_cast<double>(project.selectedMotifSlot)},
                {"slots", std::move(motifSlots)},
                {"locks", encodeMotifLocks(project.motifLocks)}
            }},
            {"formDirector", encodeFormDirector(project.formDirector)},
            {"trace", encodeControlTrace(project.controlTrace)}
        }},
        {"albumProject", std::move(albumProject)}
    });
    return serializeJson(root);
}

ProjectStateV2 decodeProjectJson(std::string_view text)
{
    const auto root = parseJson(text);
    if (!root.isObject() || child(&root, "format") == nullptr
        || child(&root, "format")->string() != "navalha-project")
        throw std::invalid_argument("File is not a Navalha project");
    const auto version = static_cast<int>(finiteNumber(child(&root, "version"), 0.0));
    if (version != 1 && version != 2)
        throw std::invalid_argument("Unsupported Navalha project version");

    ProjectStateV2 project;
    const auto* sources = child(&root, "sources");
    const auto* sourceA = version == 2 ? child(sources, "A") : &root;
    const auto* sourceB = version == 2 ? child(sources, "B")
        : child(child(&root, "dualMaterial"), "sourceB");
    const auto* slicingA = child(sourceA, "slicing");
    decodeSlices(slicingA, project.sources[0].sliceBank);
    decodeNamedSliceBanks(slicingA, project.formSliceBanks[0]);
    if (sourceB != nullptr)
    {
        const auto* slicingB = child(sourceB, "slicing");
        decodeSlices(slicingB, project.sources[1].sliceBank);
        decodeNamedSliceBanks(slicingB, project.formSliceBanks[1]);
    }
    const std::array<const Json*, 2> decodedSources {sourceA, sourceB};
    for (std::size_t index = 0; index < decodedSources.size(); ++index)
    {
        const auto* sample = child(decodedSources[index], "sample");
        auto& reference = project.sourceReferences[index];
        if (const auto* value = child(sample, "filename"); value != nullptr)
            reference.filename = value->string();
        if (const auto* value = child(sample, "relativePath"); value != nullptr)
            reference.relativePath = value->string();
        reference.size = static_cast<std::uint64_t>(std::max(
            0.0, finiteNumber(child(sample, "size"), 0.0)));
        reference.lastModified = static_cast<std::uint64_t>(std::max(
            0.0, finiteNumber(child(sample, "lastModified"), 0.0)));
        if (const auto* value = child(sample, "type"); value != nullptr)
            reference.mediaType = value->string();
    }
    const auto* savedActiveSource = version == 2
        ? child(&root, "activeSource") : child(child(&root, "dualMaterial"), "activeSource");
    project.activeSource = savedActiveSource != nullptr
        && savedActiveSource->string() == "B" && sourceB != nullptr ? 1 : 0;

    const auto* sequencer = child(&root, "sequencer");
    if (const auto* patterns = child(sequencer, "patterns");
        patterns != nullptr && patterns->isArray())
    {
        for (std::size_t row = 0; row < std::min(patterns->array().size(), patternCount); ++row)
        {
            if (!patterns->array()[row].isArray()) continue;
            for (std::size_t column = 0;
                 column < std::min(patterns->array()[row].array().size(), stepsPerPattern);
                 ++column)
                project.patterns.setCell(row, column, static_cast<std::uint16_t>(std::clamp(
                    finiteNumber(&patterns->array()[row].array()[column], 0.0), 0.0, 256.0)));
        }
    }
    project.currentPattern = static_cast<std::size_t>(std::clamp(
        finiteNumber(child(sequencer, "currentPattern"), 1.0) - 1.0, 0.0, 9.0));
    project.bpm = std::clamp(finiteNumber(child(sequencer, "bpm"), 120.0), 20.0, 400.0);
    project.divisionMode = static_cast<std::size_t>(std::clamp(
        finiteNumber(child(sequencer, "tmode"), 0.0), 0.0, 3.0));

    const auto* dsp = child(&root, "dsp");
    project.heritagePitchSemitones = static_cast<int>(std::clamp(
        finiteNumber(child(dsp, "pitch"), 0.0), -12.0, 11.0));
    project.heritagePitchMode = std::clamp(
        finiteNumber(child(dsp, "pitchMode"), 0.0), 0.0, 1.0);
    project.masterLevel = std::clamp(finiteNumber(child(dsp, "gain"), 0.8), 0.0, 1.0);
    const auto* mixer = child(dsp, "sourceMixer");
    project.mixer.sourceA = decodeMixerChannel(child(mixer, "A"));
    project.mixer.sourceB = decodeMixerChannel(child(mixer, "B"));
    project.mixer.balance = finiteNumber(child(mixer, "balance"), 0.0);
    project.mixer.normalize();
    if (const auto* voices = child(dsp, "virtualVoices");
        voices != nullptr && voices->isArray())
        for (std::size_t index = 0; index < std::min<std::size_t>(2, voices->array().size()); ++index)
            project.virtualVoices[index] = decodeVoice(voices->array()[index]);

    const auto* gesture = child(&root, "gesture");
    if (const auto* memory = child(gesture, "memory");
        memory != nullptr && memory->isArray())
    {
        for (std::size_t row = 0;
             row < std::min(memory->array().size(), patternCount); ++row)
        {
            if (!memory->array()[row].isArray())
                continue;
            for (std::size_t step = 0;
                 step < std::min(memory->array()[row].array().size(),
                                 stepsPerPattern); ++step)
                project.patternMemory[row][step] =
                    memory->array()[row].array()[step].boolean();
        }
    }
    if (const auto* transform = child(gesture, "transform");
        version == 2 && transform != nullptr)
    {
        if (const auto* base = child(transform, "base");
            base != nullptr && base->isArray())
        {
            project.patternTransform.hasBase = true;
            project.patternTransform.patternIndex =
                static_cast<std::size_t>(std::clamp(
                    finiteNumber(child(transform, "patternIndex"), 0.0),
                    0.0, 9.0));
            for (std::size_t step = 0; step < stepsPerPattern; ++step)
                project.patternTransform.base[step] =
                    step < base->array().size()
                    ? static_cast<std::uint16_t>(std::clamp(
                        finiteNumber(&base->array()[step], 0.0),
                        0.0, 256.0))
                    : 0;
            project.patternTransform.amounts = {
                static_cast<int>(std::clamp(
                    finiteNumber(child(transform, "mutationAmount"), 0.0),
                    0.0, 100.0)),
                static_cast<int>(std::clamp(
                    finiteNumber(child(transform, "erosionAmount"), 0.0),
                    0.0, 100.0)),
                static_cast<int>(std::clamp(
                    finiteNumber(child(transform, "deconstructAmount"), 0.0),
                    0.0, 100.0))
            };
        }
    }
    project.formDirector = decodeFormDirector(child(gesture, "formDirector"));
    if (const auto* tracePoints = child(child(gesture, "trace"), "points");
        tracePoints != nullptr && tracePoints->isArray())
    {
        std::array<ControlTracePoint, maxTracePoints> points {};
        const auto count = std::min<std::size_t>(
            tracePoints->array().size(), maxTracePoints);
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto& point = tracePoints->array()[index];
            points[index] = {
                static_cast<std::uint32_t>(std::max(
                    0.0, finiteNumber(child(&point, "t"), 0.0))),
                static_cast<int>(finiteNumber(child(&point, "bpm"), 120.0)),
                static_cast<int>(finiteNumber(child(&point, "pitch"), 0.0))
            };
        }
        project.controlTrace.restore(
            std::span<const ControlTracePoint>(points.data(), count));
    }
    const auto* timing = child(gesture, "timing");
    const auto mode = child(timing, "mode") == nullptr
        ? std::string_view("GRID") : child(timing, "mode")->string();
    project.timingMode = mode == "FREE" ? TimingMode::free
        : mode == "JITTER" ? TimingMode::jitter : TimingMode::grid;
    project.jitter = std::clamp(finiteNumber(child(timing, "jitter"), 18.0), 0.0, 40.0);
    project.timingSeed = static_cast<std::uint32_t>(std::clamp(
        finiteNumber(child(timing, "seed"), AssistedRng::defaultSeed),
        0.0, 4294967295.0));
    const auto* assisted = child(gesture, "assistedPerformer");
    std::uint32_t seed = AssistedRng::defaultSeed;
    if (child(assisted, "decisionSeed") != nullptr)
        static_cast<void>(AssistedRng::parseSeed(child(assisted, "decisionSeed")->string(), seed));
    project.assistedSeed = seed;
    project.assistedState = static_cast<std::uint32_t>(std::clamp(
        finiteNumber(child(assisted, "decisionState"), seed), 0.0, 4294967295.0));
    project.assistedCursor = static_cast<std::uint64_t>(std::max(
        0.0, finiteNumber(child(assisted, "decisionCursor"), 0.0)));
    project.assisted.enabled = child(assisted, "enabled") != nullptr
        && child(assisted, "enabled")->boolean();
    project.assisted.repeat = child(assisted, "repeat") == nullptr
        || child(assisted, "repeat")->boolean();
    project.assisted.chooseSource = child(assisted, "chooseSource") == nullptr
        || child(assisted, "chooseSource")->boolean();
    project.assisted.changeOrder = child(assisted, "changeOrder") == nullptr
        || child(assisted, "changeOrder")->boolean();
    project.assisted.editRegion = child(assisted, "editRegion") == nullptr
        || child(assisted, "editRegion")->boolean();
    project.assisted.editSlices = child(assisted, "editSlices") != nullptr
        && child(assisted, "editSlices")->boolean();
    project.assisted.autoMix =
        (child(assisted, "autoMix") != nullptr
         && child(assisted, "autoMix")->boolean())
        || (child(child(mixer, "automation"), "enabled") != nullptr
            && child(child(mixer, "automation"), "enabled")->boolean());
    project.assisted.applyTransform =
        child(assisted, "applyTransform") == nullptr
        || child(assisted, "applyTransform")->boolean();
    project.assisted.useGaps = child(assisted, "useGaps") != nullptr
        && child(assisted, "useGaps")->boolean();
    project.assisted.changePitch = child(assisted, "changePitch") == nullptr
        || child(assisted, "changePitch")->boolean();
    project.assisted.useFragments = child(assisted, "useFragments") == nullptr
        || child(assisted, "useFragments")->boolean();
    project.assisted.minBpm = static_cast<int>(
        finiteNumber(child(assisted, "minBpm"), 72.0));
    project.assisted.maxBpm = static_cast<int>(
        finiteNumber(child(assisted, "maxBpm"), 144.0));
    project.assisted.variation = static_cast<int>(
        finiteNumber(child(assisted, "variation"), 48.0));
    normalizeAssistedSettings(project.assisted);
    const auto* motifMemory = child(gesture, "motifMemory");
    project.selectedMotifSlot = static_cast<std::size_t>(std::clamp(
        finiteNumber(child(motifMemory, "selected"), 0.0),
        0.0, static_cast<double>(motifSlotCount - 1)));
    if (const auto* slots = child(motifMemory, "slots");
        slots != nullptr && slots->isArray())
        for (std::size_t index = 0;
             index < std::min(slots->array().size(), motifSlotCount); ++index)
            project.motifSlots[index] = decodeMotif(
                slots->array()[index], index);
    project.motifLocks = decodeMotifLocks(child(motifMemory, "locks"));
    if (const auto* albumProject = child(&root, "albumProject");
        albumProject != nullptr && albumProject->isObject())
    {
        project.albumProject = decodeAlbumProject(serializeJson(*albumProject));
        project.hasAlbumProject = true;
    }
    return project;
}
}
