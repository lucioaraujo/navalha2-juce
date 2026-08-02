#include "core/ProjectState.h"

#include <algorithm>

namespace navalha
{
ProjectStateV2 captureProjectState(const SessionModel& session) noexcept
{
    ProjectStateV2 project;
    project.sources = session.sources;
    project.patterns = session.patterns;
    project.mixer = session.mixer;
    project.virtualVoices = session.virtualVoices;
    project.patternMemory = session.patternMemory;
    project.patternTransform = session.patternTransform;
    project.formDirector = session.formDirector.state();
    project.controlTrace = session.controlTrace;
    project.activeSource = session.activeSource;
    project.currentPattern = session.sequencer.currentPattern();
    project.bpm = session.sequencer.tempo();
    project.divisionMode = session.sequencer.division();
    project.timingMode = session.sequencer.timing();
    project.jitter = session.sequencer.jitterPercent();
    project.timingSeed = session.sequencer.seed();
    project.heritagePitchSemitones = session.heritagePitchSemitones;
    project.heritagePitchMode = session.heritagePitchMode;
    project.masterLevel = session.masterLevel;
    project.assistedSeed = session.assistedRng.seed();
    project.assistedState = session.assistedRng.state();
    project.assistedCursor = session.assistedRng.cursor();
    project.assisted = session.assisted;
    project.motifLocks = session.motifLocks;
    return project;
}

ProjectStateV2 migrateProjectV1(const ProjectStateV1& legacy) noexcept
{
    ProjectStateV2 project;
    project.sources[0] = legacy.sourceA;
    if (legacy.hasDualMaterialExtension)
        project.sources[1] = legacy.sourceB;
    project.patterns = legacy.patterns;
    project.activeSource = legacy.hasDualMaterialExtension
        ? std::min<std::size_t>(legacy.activeSource, 1) : 0;
    project.currentPattern = std::min(legacy.currentPattern, patternCount - 1);
    project.bpm = std::clamp(legacy.bpm, 20.0, 400.0);
    project.divisionMode = std::min<std::size_t>(legacy.divisionMode, 3);
    project.heritagePitchSemitones = std::clamp(legacy.pitchSemitones, -12, 11);
    project.heritagePitchMode = std::clamp(legacy.pitchMode, 0.0, 1.0);
    project.masterLevel = std::clamp(legacy.masterLevel, 0.0, 1.0);
    return project;
}

void restoreProjectState(const ProjectStateV2& project, SessionModel& session)
{
    session.sequencer.stop();
    session.sources = project.sources;
    session.patterns = project.patterns;
    session.mixer = project.mixer;
    session.mixer.normalize();
    session.virtualVoices = project.virtualVoices;
    session.patternMemory = project.patternMemory;
    session.patternTransform = project.patternTransform;
    session.formDirector.restore(project.formDirector);
    session.controlTrace = project.controlTrace;
    session.patternTransform.patternIndex = std::min(
        session.patternTransform.patternIndex, patternCount - 1);
    session.patternTransform.amounts.mutation = std::clamp(
        session.patternTransform.amounts.mutation, 0, 100);
    session.patternTransform.amounts.erosion = std::clamp(
        session.patternTransform.amounts.erosion, 0, 100);
    session.patternTransform.amounts.deconstruct = std::clamp(
        session.patternTransform.amounts.deconstruct, 0, 100);
    for (auto& cell : session.patternTransform.base)
        cell = std::min<std::uint16_t>(cell, gapCellCode);
    session.activeSource = std::min<std::size_t>(project.activeSource, 1);
    session.masterLevel = std::clamp(project.masterLevel, 0.0, 1.0);
    session.heritagePitchSemitones = std::clamp(project.heritagePitchSemitones, -12, 11);
    session.heritagePitchMode = std::clamp(project.heritagePitchMode, 0.0, 1.0);
    session.assistedRng.restore(
        project.assistedSeed, project.assistedState, project.assistedCursor);
    session.assisted = project.assisted;
    normalizeAssistedSettings(session.assisted);
    // Match the v0.28.1 safety contract: settings persist, but automation
    // never resumes armed merely because a project was opened.
    session.assisted.enabled = false;
    session.motifLocks = project.motifLocks;
    session.sequencer.setTempo(std::clamp(project.bpm, 20.0, 400.0),
                               std::min<std::size_t>(project.divisionMode, 3));
    session.sequencer.setTiming(project.timingMode,
                                std::clamp(project.jitter, 0.0, 40.0),
                                project.timingSeed);
    session.sequencer.selectPattern(
        std::min(project.currentPattern, patternCount - 1));
    session.sequencer.reset();
}
}
