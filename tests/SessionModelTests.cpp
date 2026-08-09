#include "core/AlbumProject.h"
#include "core/AssistedRng.h"
#include "core/AssistedPerformer.h"
#include "core/AudioComparison.h"
#include "core/AudioEngine.h"
#include "core/ControlTrace.h"
#include "core/FragmentGesture.h"
#include "core/FormDirector.h"
#include "core/HeritagePitch.h"
#include "core/Json.h"
#include "core/LegacyFormat.h"
#include "core/LookaheadLimiter.h"
#include "core/MasteringAnalysis.h"
#include "core/MasteringAlbum.h"
#include "core/MasteringAlbumManifest.h"
#include "core/MasteringProcessor.h"
#include "core/MasteringRecipe.h"
#include "core/OfflineRenderer.h"
#include "core/OutputStage.h"
#include "core/PatternTransform.h"
#include "core/PortablePath.h"
#include "core/PortableArchive.h"
#include "core/PortableProject.h"
#include "core/ProjectState.h"
#include "core/ProjectJson.h"
#include "core/RecordingWriterService.h"
#include "core/SessionModel.h"
#include "core/SlicePlayer.h"
#include "core/StereoMixer.h"
#include "core/TakeCatalog.h"
#include "core/TruePeakDetector.h"
#include "core/WavMemoryReader.h"
#include "core/WavMetadataRewriter.h"
#include "core/WavStreamWriter.h"
#include "core/WaveformPeaks.h"
#include "validation/TruePeakFixtures.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool approximately(double left, double right)
{
    return std::abs(left - right) < 1.0e-12;
}

double measureTruePeak(const std::vector<float>& signal)
{
    navalha::TruePeakDetector detector;
    detector.prepare(48000.0);
    double maximum = 0.0;
    for (const auto sample : signal)
        maximum = std::max(
            maximum, static_cast<double>(detector.processSample(sample)));
    return 20.0 * std::log10(maximum);
}

navalha::LookaheadLimiterTelemetry measureLimitedTruePeak(
    const std::vector<float>& signal)
{
    navalha::LookaheadLimiter limiter;
    limiter.prepare(48000.0);
    for (const auto sample : signal)
        static_cast<void>(limiter.process({sample, sample}));
    for (std::size_t frame = 0;
         frame < limiter.latencySamples() + 128; ++frame)
        static_cast<void>(limiter.process({}));
    return limiter.telemetry();
}

std::uint32_t readU32(const std::string& data, std::size_t offset)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset]))
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 3])) << 24U);
}

std::size_t findRiffChunk(const std::string& data, std::string_view id)
{
    if (data.size() < 12 || id.size() != 4)
        return std::string::npos;
    const auto declaredEnd = static_cast<std::uint64_t>(readU32(data, 4)) + 8U;
    if (declaredEnd > data.size())
        return std::string::npos;
    auto offset = std::size_t {12};
    while (offset + 8 <= declaredEnd)
    {
        const auto chunkBytes = static_cast<std::uint64_t>(readU32(data, offset + 4));
        const auto paddedBytes = chunkBytes + (chunkBytes & 1U);
        if (paddedBytes > declaredEnd - offset - 8U)
            return std::string::npos;
        if (data.compare(offset, 4, id) == 0)
            return offset;
        offset += static_cast<std::size_t>(8U + paddedBytes);
    }
    return std::string::npos;
}

std::int16_t readI16(const std::string& data, std::size_t offset)
{
    const auto bits = static_cast<std::uint16_t>(
        static_cast<unsigned char>(data[offset]))
        | static_cast<std::uint16_t>(
            static_cast<unsigned char>(data[offset + 1])) << 8U;
    return static_cast<std::int16_t>(bits);
}
}

int main()
{
    using namespace navalha;

    SessionModel session;
    require(session.sources[0].sliceBank.size() == 8, "Source A must start with 8 slices");
    require(session.sources[1].sliceBank.size() == 8, "Source B must start with 8 slices");

    session.sources[0].sliceBank.divideRegion(0.25, 0.75, 4);
    const auto& divided = session.sources[0].sliceBank.slices();
    require(divided.size() == 4, "Region division must create four slices");
    require(approximately(divided.front().start, 0.25), "First slice start mismatch");
    require(approximately(divided.back().end, 0.75), "Last slice end mismatch");

    session.sources[0].sliceBank.setSlice(1, {0.38, 0.51});
    require(approximately(session.sources[0].sliceBank.slices()[1].start, 0.38),
            "Edited slice must be stored");

    session.sources[1].sliceBank.divideRegion(0.2, 0.8, 8);
    require(session.sources[1].sliceBank.addBladeCut(0.47),
            "BLADE must accept a cut inside the selected region");
    require(session.sources[1].sliceBank.size() == 9,
            "BLADE must preserve existing divisions while adding a slice");
    require(session.sources[1].sliceBank.addBladeCut(0.39),
            "BLADE must accept cuts in performance order");
    const auto bladeSlices = session.sources[1].sliceBank.slices();
    for (std::size_t index = 1; index < bladeSlices.size(); ++index)
        require(approximately(bladeSlices[index - 1].end, bladeSlices[index].start),
                "BLADE boundaries must remain sorted and contiguous");
    require(session.sources[1].sliceBank.undoBladeCut()
                && session.sources[1].sliceBank.bladeCutCount() == 1,
            "Undo BLADE must remove the most recently added cut");
    require(!session.sources[1].sliceBank.addBladeCut(0.47),
            "BLADE must reject duplicate boundaries");

    session.mixer.sourceA.level = 1.0;
    session.mixer.sourceB.level = 0.8;
    session.mixer.balance = 0.25;
    require(approximately(session.mixer.effectiveLevel(0), 0.75),
            "Positive balance must attenuate A");
    require(approximately(session.mixer.effectiveLevel(1), 0.8),
            "Positive balance must preserve B");

    session.mixer.sourceA.solo = true;
    require(approximately(session.mixer.effectiveLevel(1), 0.0),
            "Solo A must silence B without replacing its stored level");
    session.mixer.sourceA.solo = false;

    session.mixer.sourceB.muted = true;
    require(approximately(session.mixer.effectiveLevel(1), 0.0),
            "Mute must silence the selected source");

    session.selectSource(1);
    require(session.activeSource == 1, "Source B selection mismatch");

    require(session.patterns.cell(0, 7) == 7,
            "Patterns must default to the historical ascending row");
    session.patterns.setCell(0, 0, 131);
    session.patterns.setCell(0, 1, gapCellCode);
    const auto sourceBCell = PatternCell::decode(session.patterns.cell(0, 0));
    require(sourceBCell.kind == PatternCell::Kind::sourceB && sourceBCell.sliceIndex == 3,
            "Code 131 must address SOURCE B slice 3");
    require(PatternCell::decode(gapCellCode).kind == PatternCell::Kind::gap,
            "Code 256 must represent GAP");

    session.sequencer.prepare(48000.0);
    session.sequencer.setTempo(120.0, 0);
    session.sequencer.start();

    std::size_t eventCount = 0;
    std::size_t secondEventSample = 0;
    for (std::size_t sample = 0; sample <= 24000; ++sample)
    {
        if (const auto event = session.sequencer.processSample())
        {
            if (eventCount == 0)
                require(event->cell.kind == PatternCell::Kind::sourceB,
                        "The first sequencer event must use the selected pattern cell");
            if (eventCount == 1)
            {
                secondEventSample = sample;
                require(event->cell.kind == PatternCell::Kind::gap,
                        "The second sequencer event must preserve GAP");
            }
            ++eventCount;
        }
    }
    require(eventCount == 2 && secondEventSample == 24000,
            "120 BPM GRID events must be exactly 24000 samples apart at 48 kHz");

    PatternBank timingPatterns;
    Sequencer jitterA(timingPatterns);
    Sequencer jitterB(timingPatterns);
    jitterA.prepare(1000.0);
    jitterB.prepare(1000.0);
    jitterA.setTempo(120.0, 0);
    jitterB.setTempo(120.0, 0);
    jitterA.setTiming(TimingMode::jitter, 18.0, 0x12345678U);
    jitterB.setTiming(TimingMode::jitter, 18.0, 0x12345678U);
    jitterA.start();
    jitterB.start();
    for (std::size_t sample = 0; sample < 4000; ++sample)
    {
        const auto eventA = jitterA.processSample();
        const auto eventB = jitterB.processSample();
        require(eventA.has_value() == eventB.has_value(),
                "Equal timing seeds must produce identical JITTER event positions");
    }

    const auto generationBeforeStop = session.sequencer.generation();
    session.sequencer.stop();
    require(!session.sequencer.isRunning()
                && session.sequencer.generation() == generationBeforeStop + 1,
            "STOP must halt playback and invalidate pending transport work");
    for (std::size_t sample = 0; sample < 48000; ++sample)
        require(!session.sequencer.processSample(), "STOP must suppress all later events");

    session.sequencer.reset();
    require(session.sequencer.currentStep() == 0, "RESET must return to the first step");

    bool rejected = false;
    try
    {
        session.sources[0].sliceBank.divideRegion(0.0, 1.0, maxSlices + 1);
    }
    catch (const std::out_of_range&)
    {
        rejected = true;
    }
    require(rejected, "Banks larger than 128 slices must be rejected");

    rejected = false;
    try
    {
        session.patterns.setCell(0, 0, gapCellCode + 1);
    }
    catch (const std::out_of_range&)
    {
        rejected = true;
    }
    require(rejected, "Pattern codes larger than GAP must be rejected");

    const Pattern transformBase {0, 5, 130, 135, 8, 140, gapCellCode, 2};
    const PatternMemory transformMemory {
        false, false, true, false, false, false, true, false
    };
    require(transformPattern(
                transformBase, transformMemory, {12, 7}, 3, {0, 0, 0})
                == transformBase,
            "Zero structural transforms must preserve the reversible base");
    require(transformPattern(
                transformBase, transformMemory, {12, 7}, 3, {73, 41, 66})
                == Pattern {8, 132, 130, 132, 130, gapCellCode,
                            gapCellCode, 130},
            "Pattern transforms must match the v0.28.1 JavaScript fixture");
    require(transformPattern(
                transformBase, transformMemory, {12, 7}, 3,
                {73, 41, 66}, false)
                == Pattern {8, 8, 130, 132, 130, 134, gapCellCode, 2},
            "Gap-locked transforms must use source-aware replacements");
    PatternMemory protectEverything {};
    protectEverything.fill(true);
    require(transformPattern(
                transformBase, protectEverything, {12, 7}, 3,
                {100, 100, 100})
                == transformBase,
            "MEMORY must protect every marked cell from all transforms");
    rejected = false;
    try
    {
        static_cast<void>(transformPattern(
            transformBase, transformMemory, {12, 7}, 3, {101, 0, 0}));
    }
    catch (const std::out_of_range&)
    {
        rejected = true;
    }
    require(rejected, "Structural transforms must enforce 0-100 amounts");
    SessionModel transformSession;
    transformSession.patterns.setPattern(3, transformBase);
    transformSession.sources[0].sliceBank.divideRegion(0.0, 1.0, 12);
    transformSession.sources[1].sliceBank.divideRegion(0.0, 1.0, 7);
    require(transformSession.togglePatternMemory(3, 2)
                && transformSession.togglePatternMemory(3, 6),
            "MEMORY toggles must mark individual pattern cells");
    transformSession.applyPatternTransform(3, {73, 41, 66});
    require(transformSession.patterns.pattern(3)
                == Pattern {8, 132, 130, 132, 130, gapCellCode,
                            gapCellCode, 130}
                && transformSession.patternTransform.hasBase,
            "Transform session must rebuild from a reversible base");
    transformSession.restorePatternTransform();
    require(transformSession.patterns.pattern(3) == transformBase
                && !transformSession.patternTransform.hasBase,
            "RESTORE must recover the exact pre-transform pattern");
    transformSession.applyPatternTransform(3, {73, 0, 0});
    const auto committedPattern = transformSession.patterns.pattern(3);
    transformSession.commitPatternTransform();
    require(transformSession.patterns.pattern(3) == committedPattern
                && !transformSession.patternTransform.hasBase,
            "COMMIT must keep the transformed pattern and discard its base");

    FormDirector form;
    require(form.state().sceneCount == 5
                && formText(form.state().scenes[0].name) == "INTRO"
                && form.state().scenes[1].bars == 8
                && form.state().scenes[3].energy == 92
                && form.state().scenes[4].transition
                    == FormTransition::dissolve,
            "FORM must reproduce the five v0.28.1 default scenes");
    form.setEnabled(true);
    for (int bar = 0; bar < 4; ++bar)
        static_cast<void>(form.notePhraseCompleted());
    require(form.state().currentScene == 1 && form.state().bar == 0,
            "FORM must advance after the selected scene bar count");
    auto editedScene = form.state().scenes[1];
    editedScene.energy = 107;
    editedScene.name = makeFormText(std::string(50, 'X'));
    require(form.replaceCurrentScene(editedScene)
                && form.state().scenes[1].energy == 100
                && formText(form.state().scenes[1].name).size() == 36,
            "FORM scene edits must enforce browser-compatible limits");
    require(form.toggleCurrentLock()
                && !form.replaceCurrentScene(FormScene {}),
            "FORM scene lock must prevent editing");
    require(form.duplicateScene() && form.state().sceneCount == 6
                && formText(
                    form.state().scenes[form.state().currentScene].name)
                    == "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
            "FORM duplication must preserve scene data and clear its lock");
    require(form.deleteScene() && form.state().sceneCount == 5,
            "FORM scene deletion must retain at least one scene");
    FormDirector formHistory;
    auto namedScene = formHistory.state().scenes[0];
    namedScene.name = makeFormText("OPENING KNIFE");
    require(formHistory.replaceCurrentScene(namedScene, true)
                && formHistory.canUndo() && !formHistory.canRedo()
                && formHistory.addScene()
                && formHistory.state().sceneCount == 6,
            "FORM structural edits must enter a bounded undo history");
    require(formHistory.undoEdit()
                && formHistory.state().sceneCount == 5
                && formText(formHistory.state().scenes[0].name)
                    == "OPENING KNIFE"
                && formHistory.canRedo(),
            "FORM undo must restore the previous scene structure and names");
    require(formHistory.redoEdit()
                && formHistory.state().sceneCount == 6
                && !formHistory.canRedo(),
            "FORM redo must restore the reverted structural edit");
    FormDirector boundedFormHistory;
    for (std::size_t edit = 0; edit < maxFormHistory + 6; ++edit)
        static_cast<void>(boundedFormHistory.toggleCurrentLock());
    for (std::size_t edit = 0; edit < maxFormHistory; ++edit)
        require(boundedFormHistory.undoEdit(),
                "FORM must retain its advertised fixed history depth");
    require(!boundedFormHistory.undoEdit(),
            "FORM history must discard edits older than its fixed capacity");
    const auto context = assistedPerformanceContext(form.state(), 60.0);
    require(context.formActive
                && approximately(context.intensity, 0.785)
                && approximately(context.energy, 0.69),
            "FORM must shape Assisted intensity/energy with the web equations");
    require(generatedSliceCount(SliceBankProfile::longSlices) == 4
                && generatedSliceCount(SliceBankProfile::medium) == 8
                && generatedSliceCount(SliceBankProfile::shortSlices) == 16
                && generatedSliceCount(SliceBankProfile::micro) == 32
                && generatedSliceCount(SliceBankProfile::working) == 0,
            "FORM named slice banks must match v0.28.1 generated counts");
    SessionModel formSession;
    formSession.sources[0].sliceBank.divideRegion(0.2, 0.8, 8);
    formSession.sources[1].sliceBank.divideRegion(0.1, 0.9, 8);
    formSession.formDirector.setEnabled(true);
    formSession.applyCurrentFormSceneMaterial();
    require(formSession.sources[0].sliceBank.size() == 4
                && approximately(
                    formSession.sources[0].sliceBank.slices().front().start, 0.2)
                && approximately(
                    formSession.sources[0].sliceBank.slices().back().end, 0.8),
            "FORM LONG bank must preserve the selected source region");
    static_cast<void>(formSession.formDirector.selectScene(3));
    formSession.applyCurrentFormSceneMaterial();
    require(formSession.sources[0].sliceBank.size() == 32
                && formSession.sources[1].sliceBank.size() == 16,
            "FORM CLIMAX must apply MICRO A and SHORT B banks");
    formSession.sources[0].sliceBank.divideRegion(0.3, 0.7, 3);
    require(formSession.captureFormSliceBank(
                0, SliceBankProfile::longSlices)
                == SliceBankProfile::longSlices,
            "FORM capture must overwrite the selected named bank");
    static_cast<void>(formSession.formDirector.selectScene(0));
    formSession.applyCurrentFormSceneMaterial();
    require(formSession.sources[0].sliceBank.size() == 3
                && approximately(
                    formSession.sources[0].sliceBank.slices().front().start, 0.3)
                && approximately(
                    formSession.sources[0].sliceBank.slices().back().end, 0.7),
            "FORM scenes must recall captured banks instead of regenerating them");
    auto formQueueSession = std::make_unique<SessionModel>();
    formQueueSession->sources[0].sliceBank.divideRegion(0.25, 0.75, 5);
    auto formQueueEngine = std::make_unique<AudioEngine>(*formQueueSession);
    formQueueEngine->prepare(48000.0);
    require(formQueueEngine->submitCommand({
                    EngineCommandType::captureFormSliceBank, 0,
                    static_cast<std::size_t>(SliceBankProfile::working)}),
            "FORM bank capture must enter the realtime command queue");
    formQueueEngine->synchronizePendingCommands();
    require(formQueueSession->formSliceBanks[0].has(
                    SliceBankProfile::manual)
                && formQueueSession->formSliceBanks[0].bank(
                    SliceBankProfile::manual).size() == 5,
            "Audio-thread FORM commands must preserve captured banks");
    require(formQueueEngine->submitCommand({EngineCommandType::addFormScene})
                && formQueueEngine->submitCommand({EngineCommandType::undoFormEdit}),
            "FORM undo must enter the realtime command queue");
    formQueueEngine->synchronizePendingCommands();
    require(formQueueSession->formDirector.state().sceneCount == 5,
            "Queued FORM undo must restore the structural scene state");

    AssistedPerformerSettings assistedSettings;
    assistedSettings.enabled = true;
    assistedSettings.useGaps = true;
    assistedSettings.minBpm = 72;
    assistedSettings.maxBpm = 144;
    assistedSettings.variation = 68;
    AssistedRng assistedPlannerA(0x12345678U);
    AssistedRng assistedPlannerB(0x12345678U);
    AssistedPhraseInput assistedInput;
    assistedInput.currentBpm = 120;
    assistedInput.currentSource = 0;
    assistedInput.currentPattern = 3;
    assistedInput.playable = {true, true};
    assistedInput.mixer.available = true;
    for (std::size_t patternIndex = 0;
         patternIndex < patternCount; ++patternIndex)
    {
        assistedInput.patterns[patternIndex][0] =
            static_cast<std::uint16_t>(patternIndex);
        assistedInput.memory[patternIndex][0] = true;
    }
    const auto assistedDecisionA = planAssistedPhrase(
        assistedSettings, formSession.formDirector.state(),
        assistedInput, assistedPlannerA);
    const auto assistedDecisionB = planAssistedPhrase(
        assistedSettings, formSession.formDirector.state(),
        assistedInput, assistedPlannerB);
    require(assistedDecisionA.bpm == assistedDecisionB.bpm
                && assistedDecisionA.pitch == assistedDecisionB.pitch
                && assistedDecisionA.transform.mutation
                    == assistedDecisionB.transform.mutation
                && assistedDecisionA.fragment == assistedDecisionB.fragment
                && assistedPlannerA.cursor() == assistedPlannerB.cursor()
                && assistedDecisionA.bpm >= 72
                && assistedDecisionA.bpm <= 144,
            "Assisted phrase planning must be deterministic and bounded");
    auto completeSettings = assistedSettings;
    completeSettings.editSlices = true;
    completeSettings.autoMix = true;
    AssistedRng completeRandom(0x4e415632U);
    bool sawSource = false;
    bool sawPattern = false;
    bool sawRegion = false;
    bool sawCuts = false;
    bool sawMix = false;
    for (int phrase = 0; phrase < 256; ++phrase)
    {
        const auto decision = planAssistedPhrase(
            completeSettings, formSession.formDirector.state(),
            assistedInput, completeRandom);
        sawSource = sawSource || decision.switchesSource;
        sawPattern = sawPattern || decision.selectsPattern;
        sawRegion = sawRegion || decision.editsRegion;
        sawCuts = sawCuts || decision.cutAction != AssistedCutAction::none;
        sawMix = sawMix || decision.mixes;
        require(decision.source < 2 && decision.pattern < patternCount
                    && decision.bpm >= 72 && decision.bpm <= 144,
                "Assisted structural decisions must remain bounded");
        require(decision.patternRow[0]
                    == assistedInput.patterns[decision.pattern][0],
                "Assisted pattern recombination must preserve MEMORY cells");
        if (decision.editsRegion)
            require(decision.region.start >= 0.0
                        && decision.region.end <= 1.0
                        && decision.region.end > decision.region.start,
                    "Assisted region edits must remain valid");
        if (decision.mixes)
            require(std::abs(decision.mixer.balance) <= 0.52
                        && std::abs(decision.mixer.pan[0]) <= 0.48
                        && std::abs(decision.mixer.pan[1]) <= 0.48,
                    "AUTO MIX must remain spatially conservative");
    }
    require(sawSource && sawPattern && sawRegion && sawCuts && sawMix,
            "Assisted planner must cover source, pattern, region, cuts and mix");
    AssistedPerformerSettings unsafeAssisted;
    unsafeAssisted.minBpm = 500;
    unsafeAssisted.maxBpm = 10;
    unsafeAssisted.variation = 200;
    normalizeAssistedSettings(unsafeAssisted);
    require(unsafeAssisted.minBpm == 20
                && unsafeAssisted.maxBpm == 400
                && unsafeAssisted.variation == 100,
            "Assisted settings must normalize web-compatible ranges");
    SessionModel assistedEngineSession;
    assistedEngineSession.assisted.enabled = true;
    assistedEngineSession.assisted.chooseSource = false;
    assistedEngineSession.assisted.changeOrder = false;
    assistedEngineSession.assisted.editRegion = false;
    assistedEngineSession.assisted.editSlices = false;
    assistedEngineSession.assisted.autoMix = false;
    assistedEngineSession.assisted.applyTransform = false;
    assistedEngineSession.assisted.changePitch = false;
    assistedEngineSession.assisted.useFragments = false;
    assistedEngineSession.assisted.minBpm = 137;
    assistedEngineSession.assisted.maxBpm = 137;
    assistedEngineSession.assisted.variation = 50;
    assistedEngineSession.sequencer.setTempo(400.0, 3);
    StereoAudioBuffer assistedAudio(
        1000.0, std::vector<float>(1000, 0.1F),
        std::vector<float>(1000, 0.1F));
    AudioEngine assistedEngine(assistedEngineSession);
    assistedEngine.prepare(1000.0);
    assistedEngine.setSourceBuffer(0, &assistedAudio);
    assistedEngine.start();
    for (int frame = 0; frame < 400; ++frame)
        static_cast<void>(assistedEngine.processSample());
    require(approximately(assistedEngineSession.sequencer.tempo(), 137.0),
            "Assisted runtime must apply its phrase decision in AudioEngine");

    ControlTrace trace;
    require(trace.append(0, 120, 0)
                && !trace.append(20, 125, 1)
                && !trace.append(40, 120, 0)
                && trace.append(80, 180, -7)
                && trace.append(110, 999, -99, true)
                && trace.size() == 3
                && trace.durationMs() == 110
                && trace.points()[2].bpm == 400
                && trace.points()[2].pitch == -12,
            "TRACE must throttle duplicate control points and clamp BPM/pitch");
    ControlTracePlayer tracePlayer;
    tracePlayer.prepare(1000.0);
    require(tracePlayer.start(trace, true)
                && tracePlayer.advance() == &trace.points()[0],
            "TRACE playback must apply its first point at frame zero");
    for (int frame = 1; frame < 80; ++frame)
        require(tracePlayer.advance() == nullptr,
                "TRACE playback must wait for each sample-accurate timestamp");
    require(tracePlayer.advance() == &trace.points()[1],
            "TRACE playback must apply the next point at its exact frame");
    for (int frame = 81; frame < 130; ++frame)
        static_cast<void>(tracePlayer.advance());
    require(tracePlayer.cycleCount() == 1
                && tracePlayer.advance() == &trace.points()[0],
            "TRACE LOOP must restart after duration plus the web 20 ms gap");

    SessionModel traceEngineSession;
    require(traceEngineSession.controlTrace.append(0, 96, -4)
                && traceEngineSession.controlTrace.append(80, 180, 7),
            "TRACE engine fixture must contain two points");
    AudioEngine traceEngine(traceEngineSession);
    traceEngine.prepare(1000.0);
    require(traceEngine.submitCommand(
                {EngineCommandType::startTraceLoop, 0}),
            "TRACE playback must be startable through the realtime queue");
    static_cast<void>(traceEngine.processSample());
    require(approximately(traceEngineSession.sequencer.tempo(), 96.0)
                && traceEngineSession.heritagePitchSemitones == -4
                && approximately(traceEngineSession.heritagePitchMode, 1.0),
            "TRACE playback must apply BPM and Heritage Pitch in the engine");
    for (int frame = 1; frame <= 80; ++frame)
        static_cast<void>(traceEngine.processSample());
    require(approximately(traceEngineSession.sequencer.tempo(), 180.0)
                && traceEngineSession.heritagePitchSemitones == 7,
            "TRACE playback must apply later points at exact engine frames");

    const auto stutterPlan = planStutter(130, 120.0, 0, 48000.0);
    require(stutterPlan.count == 4
                && stutterPlan.cells[0] == 130
                && stutterPlan.cells[3] == 130
                && stutterPlan.frameOffsets[0] == 0
                && stutterPlan.frameOffsets[1] == 6000
                && stutterPlan.frameOffsets[3] == 18000,
            "STUTTER x4 must use quarter-step sample-accurate spacing");
    AssistedRng burstRandom(0x12345678U);
    const auto burstPlan = planBurst(
        transformBase, 0, {12, 7}, 120.0, 0, 48000.0, burstRandom);
    require(burstPlan.count == 8
                && burstPlan.cells
                    == std::array<std::uint16_t, 8> {
                        135, 5, 130, 2, 8, 140, 0, 8}
                && burstPlan.frameOffsets[1] == 3000
                && burstPlan.frameOffsets[7] == 21000,
            "BURST x8 must match the injected JavaScript shuffle fixture");
    Pattern allGaps {};
    allGaps.fill(gapCellCode);
    AssistedRng fallbackBurstRandom(0x12345678U);
    const auto fallbackBurst = planBurst(
        allGaps, 1, {12, 3}, 400.0, 3, 48000.0,
        fallbackBurstRandom);
    require(fallbackBurst.count == 8
                && std::all_of(
                    fallbackBurst.cells.begin(), fallbackBurst.cells.end(),
                    [](std::uint16_t code) { return code >= 128 && code <= 130; })
                && fallbackBurst.frameOffsets[1] == 864,
            "BURST must fall back to active-source slices and clamp spacing");

    std::vector<float> gestureRamp(800);
    for (std::size_t frame = 0; frame < gestureRamp.size(); ++frame)
        gestureRamp[frame] = static_cast<float>(frame)
            / static_cast<float>(gestureRamp.size());
    const StereoAudioBuffer gestureSource(
        48000.0, gestureRamp, gestureRamp);
    const auto renderManualGesture = [&] (bool reverse)
    {
        SessionModel gestureSession;
        AudioEngine gestureEngine(gestureSession);
        gestureEngine.prepare(48000.0);
        gestureEngine.setSourceBuffer(0, &gestureSource);
        require(gestureEngine.submitCommand({
                    EngineCommandType::triggerSlice, 0, 0,
                    reverse ? 1.0 : 0.0}),
                "Manual slice trigger must enter the realtime queue");
        std::array<float, 40> left {};
        std::array<float, 40> right {};
        gestureEngine.processBlock(left.data(), right.data(), left.size());
        return left[30];
    };
    require(renderManualGesture(true) > renderManualGesture(false),
            "Reverse gesture must read the selected slice from its end");
    {
        SessionModel invalidGestureSession;
        AudioEngine invalidGestureEngine(invalidGestureSession);
        invalidGestureEngine.prepare(48000.0);
        require(!invalidGestureEngine.submitCommand({
                    EngineCommandType::triggerSlice, 2, 0, 1.0}),
                "Manual slice trigger must reject invalid sources");
    }

    SliceBank microBank;
    const auto microFirstIndex = microBank.size();
    require(microBank.appendMicroSlices(0, 8, 1.0) == 8
                && microBank.size() == microFirstIndex + 8
                && approximately(
                    microBank.slices()[microFirstIndex].start, 0.0)
                && approximately(
                    microBank.slices()[microFirstIndex].end, 0.015625)
                && approximately(
                    microBank.slices().back().end, 0.125),
            "MICRO x8 must append subdivisions without changing the parent");
    SliceBank nearlyFullMicroBank;
    nearlyFullMicroBank.divideRegion(0.0, 1.0, 127);
    require(nearlyFullMicroBank.appendMicroSlices(0, 8, 1.0) == 0
                && nearlyFullMicroBank.size() == 127,
            "MICRO must not exceed the fixed 128-slice capacity");
    SessionModel gestureQueueSession;
    gestureQueueSession.patterns.setPattern(3, transformBase);
    gestureQueueSession.sources[0].sliceBank.divideRegion(0.0, 1.0, 12);
    gestureQueueSession.sources[1].sliceBank.divideRegion(0.0, 1.0, 7);
    AudioEngine gestureQueueEngine(gestureQueueSession);
    gestureQueueEngine.prepare(48000.0);
    require(gestureQueueEngine.submitCommand({
                EngineCommandType::togglePatternMemory, 3, 2})
                && gestureQueueEngine.submitCommand({
                    EngineCommandType::togglePatternMemory, 3, 6})
                && gestureQueueEngine.submitCommand({
                    EngineCommandType::applyPatternTransform, 3, 1,
                    73.0, 41.0, 66.0})
                && gestureQueueEngine.submitCommand({
                    EngineCommandType::appendMicroSlices, 0, 0, 8.0, 1.0}),
            "Structural gestures must enter the realtime command queue");
    gestureQueueEngine.synchronizePendingCommands();
    require(gestureQueueSession.patterns.pattern(3)
                == Pattern {8, 132, 130, 132, 130, gapCellCode,
                            gapCellCode, 130}
                && gestureQueueSession.sources[0].sliceBank.size() == 20,
            "Audio-thread commands must apply transforms and MICRO safely");
    require(gestureQueueEngine.submitCommand({
                EngineCommandType::startStutter, 0, 0, 130.0})
                && gestureQueueEngine.submitCommand({
                    EngineCommandType::startBurst, 0}),
            "STUTTER and BURST plans must enter the realtime queue");
    gestureQueueEngine.synchronizePendingCommands();
    require(gestureQueueSession.assistedRng.cursor() == 7,
            "BURST must consume a deterministic seven-decision shuffle");
    require(!gestureQueueEngine.submitCommand({
                EngineCommandType::startStutter, 0, 0, 257.0})
                && !gestureQueueEngine.submitCommand({
                    EngineCommandType::startBurst, 2}),
            "Fragment commands must reject invalid cells and sources");

    StereoChannelProcessor channel;
    channel.prepare(48000.0);
    channel.reset();
    auto stereo = channel.process({0.75F, -0.25F});
    require(approximately(stereo.left, 0.75) && approximately(stereo.right, -0.25),
            "Neutral stereo processing must preserve the signal");

    channel.reset(1.0F, 0.0F, 0.0F);
    stereo = channel.process({0.75F, -0.25F});
    require(approximately(stereo.left, 0.25) && approximately(stereo.right, 0.25),
            "Zero width must produce the mid signal on both channels");

    channel.reset(1.0F, 1.0F, 1.0F);
    stereo = channel.process({0.75F, -0.25F});
    require(approximately(stereo.left, 0.0) && approximately(stereo.right, -0.25),
            "Full right pan must silence the left output without boosting the right");

    channel.reset(1.0F, -1.0F, 1.0F);
    stereo = channel.process({0.75F, -0.25F});
    require(approximately(stereo.left, 0.75) && approximately(stereo.right, 0.0),
            "Full left pan must silence the right output without boosting the left");

    channel.reset();
    channel.setParameters(0.0F, 0.0F, 1.0F);
    for (std::size_t sample = 0; sample < 720; ++sample)
        stereo = channel.process({1.0F, 1.0F});
    require(approximately(stereo.left, 0.0) && approximately(stereo.right, 0.0),
            "The historical 15 ms ramp must reach its target after 720 samples at 48 kHz");

    StereoSourceMixer audioMixer;
    audioMixer.prepare(48000.0);
    stereo = audioMixer.process({0.25F, 0.5F}, {0.75F, -0.5F});
    require(approximately(stereo.left, 1.0) && approximately(stereo.right, 0.0),
            "The SOURCE mixer must sum processed A and B channels");

    StereoAudioBuffer audioBuffer(
        8.0,
        std::vector<float> {0.0F, 1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F},
        std::vector<float> {0.0F, -1.0F, -2.0F, -3.0F, -4.0F, -5.0F, -6.0F, -7.0F});
    require(approximately(audioBuffer.interpolated(2.5).left, 2.5),
            "Audio buffers must support linear interpolation");

    SlicePlayer player;
    player.prepare(8.0);
    player.setBuffer(&audioBuffer);
    player.trigger({0.25, 0.75});
    require(approximately(player.normalizedPosition(), 0.25),
            "Forward playhead must begin at the slice start");
    const auto forwardFirst = player.process().left;
    require(approximately(player.normalizedPosition(), 0.375),
            "Forward playhead must advance in source coordinates");
    std::vector<float> forward;
    while (player.isPlaying())
        forward.push_back(player.process().left);
    require(forward.size() == 3, "The remaining half-slice must render three samples");
    require(approximately(forwardFirst, 0.0)
                && approximately(forward.back(), 0.0),
            "Slice playback must de-click both boundaries");
    require(approximately(forward[0], 3.0) && approximately(forward[1], 4.0),
            "Forward slice playback must preserve the interior sample order");
    require(player.normalizedPosition() < 0.0,
            "Finished playback must withdraw the waveform playhead");

    player.trigger({0.25, 0.75}, true);
    const auto reverseStart = player.normalizedPosition();
    static_cast<void>(player.process());
    require(player.normalizedPosition() < reverseStart,
            "Reverse playhead must move toward the slice start");
    std::vector<float> reversed;
    while (player.isPlaying())
        reversed.push_back(player.process().left);
    require(reversed.size() == 3, "Reverse must preserve the remaining slice duration");
    require(approximately(reversed[0], 4.0) && approximately(reversed[1], 3.0),
            "Reverse must read the same slice in the opposite direction");

    player.trigger({0.0, 1.0}, false, 2.0);
    std::size_t pitchedDuration = 0;
    while (player.isPlaying())
    {
        static_cast<void>(player.process());
        ++pitchedDuration;
    }
    require(pitchedDuration == 4,
            "Playback rate 2 must transpose up one octave and halve duration");

    player.trigger({0.0, 1.0}, false, 1.0, 0.25, 0.25);
    const auto customEnvelopeStart = player.process();
    require(approximately(customEnvelopeStart.left, 0.0),
            "Custom virtual voice envelopes must begin at silence");

    player.trigger({0.0, 1.0});
    static_cast<void>(player.process());
    player.stop();
    std::size_t stopTail = 0;
    while (player.isPlaying() && stopTail < 100)
    {
        static_cast<void>(player.process());
        ++stopTail;
    }
    require(!player.isPlaying(), "STOP must fade and terminate an active slice");

    EngineCommandQueue<4> smallQueue;
    require(smallQueue.push({EngineCommandType::start}), "A new command queue must accept data");
    require(smallQueue.push({EngineCommandType::reset}), "The queue must preserve capacity");
    require(smallQueue.push({EngineCommandType::stop}), "Capacity N stores N-1 commands");
    require(!smallQueue.push({EngineCommandType::start}), "A full queue must reject without blocking");
    EngineCommand queuedCommand;
    require(smallQueue.pop(queuedCommand) && queuedCommand.type == EngineCommandType::start,
            "The command queue must preserve FIFO order");

    SessionModel engineSession;
    engineSession.patterns.setCell(0, 0, 128);
    std::vector<float> constantLeft(800, 0.5F);
    std::vector<float> constantRight(800, -0.25F);
    StereoAudioBuffer sourceBBuffer(800.0, constantLeft, constantRight);
    AudioEngine engine(engineSession);
    engine.prepare(800.0);
    engine.setSourceBuffer(1, &sourceBBuffer);
    engineSession.sequencer.setTempo(120.0, 0);
    require(engine.submitCommand({EngineCommandType::start}),
            "The UI thread must be able to queue transport start");

    StereoSample engineOutput;
    for (std::size_t sample = 0; sample < 10; ++sample)
        engineOutput = engine.processSample();
    require(engineOutput.left > 0.0F && engineOutput.right < 0.0F,
            "The engine must route a SOURCE B pattern event through its slice player");
    const auto runningTelemetry = engine.transportTelemetry();
    require(runningTelemetry.running && runningTelemetry.generation > 0
                && runningTelemetry.sourcePlayhead[0] < 0.0
                && runningTelemetry.sourcePlayhead[1] > 0.0
                && runningTelemetry.sourcePlayhead[1] <= 1.0,
            "Audio thread must publish transport state without exposing SessionModel");

    require(engine.submitCommand({EngineCommandType::stop}),
            "The UI thread must be able to queue transport stop");
    for (std::size_t sample = 0; sample < 10; ++sample)
        engineOutput = engine.processSample();
    require(approximately(engineOutput.left, 0.0) && approximately(engineOutput.right, 0.0),
            "Engine STOP must fade active voices to silence");
    require(!engine.transportTelemetry().running,
            "STOP telemetry must become visible to the UI atomically");
    require(engine.transportTelemetry().sourcePlayhead[1] < 0.0,
            "STOP must withdraw an inactive source playhead from the UI");
    require(!engine.submitCommand(
                {EngineCommandType::setPatternCell, patternCount, 0, 0.0}),
            "Invalid UI commands must be rejected before reaching the audio thread");
    require(engine.submitCommand({EngineCommandType::selectSource, 1}),
            "Active source selection must be queueable");
    engine.synchronizePendingCommands();
    require(engineSession.activeSource == 1,
            "Active source selection must persist in shared Project state");
    require(!engine.submitCommand({EngineCommandType::selectSource, 2}),
            "Invalid active source selection must be rejected");

    std::array<float, 16> leftBlock {};
    std::array<float, 16> rightBlock {};
    engine.processBlock(leftBlock.data(), rightBlock.data(), leftBlock.size());
    require(approximately(leftBlock.back(), 0.0) && approximately(rightBlock.back(), 0.0),
            "Block processing must preserve stopped silence");

    require(engine.submitCommand(
                {EngineCommandType::divideSliceRegion, 0, 4, 0.25, 0.75}),
            "Slice division must be accepted through the UI command queue");
    require(engine.submitCommand(
                {EngineCommandType::addBladeCut, 0, 0, 0.44}),
            "BLADE must be accepted through the UI command queue");
    engine.processBlock(leftBlock.data(), rightBlock.data(), 1);
    require(engineSession.sources[0].sliceBank.size() == 5,
            "Queued BLADE must preserve four divisions and add one slice");
    require(engine.submitCommand({EngineCommandType::undoBladeCut, 0}),
            "Undo BLADE must be accepted through the UI command queue");
    engine.processBlock(leftBlock.data(), rightBlock.data(), 1);
    require(engineSession.sources[0].sliceBank.size() == 4,
            "Queued undo must remove only the last manual cut");
    require(engine.submitCommand({EngineCommandType::setMasterLevel, 0, 0, 0.0}),
            "MASTER level must be accepted through the command queue");
    engine.processBlock(leftBlock.data(), rightBlock.data(), leftBlock.size());
    require(engineSession.masterLevel == 0.0,
            "MASTER command must update the shared session state");
    require(engine.submitCommand({
                EngineCommandType::setMixerChannel, 1, 3, 0.73, -0.25, 1.4}),
            "Mixer state must be accepted through the command queue");
    engine.synchronizePendingCommands();
    require(approximately(engineSession.mixer.sourceB.level, 0.73)
                && approximately(engineSession.mixer.sourceB.pan, -0.25)
                && approximately(engineSession.mixer.sourceB.width, 1.4)
                && engineSession.mixer.sourceB.muted
                && engineSession.mixer.sourceB.solo,
            "Mixer commands must persist DSP parameters and mute/solo in Project state");
    require(!engine.submitCommand({
                EngineCommandType::setMixerChannel, 0, 0, 1.0, 2.0, 1.0}),
            "Mixer commands outside the public range must be rejected");
    require(engine.submitCommand({
                EngineCommandType::setMixerBalance, 0, 0, -0.37}),
            "Global mixer balance must be queueable");
    engine.synchronizePendingCommands();
    require(approximately(engineSession.mixer.balance, -0.37),
            "Global mixer balance must persist in Project state");
    require(!engine.submitCommand({
                EngineCommandType::setMixerBalance, 0, 0, 1.01}),
            "Mixer balance outside the public range must be rejected");
    require(engine.submitCommand({
                EngineCommandType::setVirtualVoiceProperty,
                0,
                static_cast<std::size_t>(VirtualVoiceProperty::enabled),
                1.0})
                && engine.submitCommand({
                    EngineCommandType::setVirtualVoiceProperty,
                    0,
                    static_cast<std::size_t>(VirtualVoiceProperty::source),
                    1.0})
                && engine.submitCommand({
                    EngineCommandType::setVirtualVoiceProperty,
                    0,
                    static_cast<std::size_t>(VirtualVoiceProperty::division),
                    4.0})
                && engine.submitCommand({
                    EngineCommandType::setVirtualVoiceProperty,
                    0,
                    static_cast<std::size_t>(VirtualVoiceProperty::pitch),
                    -7.0})
                && engine.submitCommand({
                    EngineCommandType::setVirtualVoiceProperty,
                    0,
                    static_cast<std::size_t>(VirtualVoiceProperty::level),
                    0.42})
                && engine.submitCommand({
                    EngineCommandType::setVirtualVoiceProperty,
                    0,
                    static_cast<std::size_t>(VirtualVoiceProperty::pan),
                    0.31})
                && engine.submitCommand({
                    EngineCommandType::setVirtualVoiceProperty,
                    0,
                    static_cast<std::size_t>(VirtualVoiceProperty::patternLength),
                    12.0})
                && engine.submitCommand({
                    EngineCommandType::setVirtualVoiceProperty,
                    0,
                    static_cast<std::size_t>(VirtualVoiceProperty::focusStart),
                    0.2})
                && engine.submitCommand({
                    EngineCommandType::setVirtualVoiceProperty,
                    0,
                    static_cast<std::size_t>(VirtualVoiceProperty::focusEnd),
                    0.85})
                && engine.submitCommand({
                    EngineCommandType::setVirtualVoiceProperty,
                    0,
                    static_cast<std::size_t>(VirtualVoiceProperty::attack),
                    0.023})
                && engine.submitCommand({
                    EngineCommandType::setVirtualVoiceProperty,
                    0,
                    static_cast<std::size_t>(VirtualVoiceProperty::release),
                    0.34})
                && engine.submitCommand({
                    EngineCommandType::setVirtualVoicePatternCell,
                    0,
                    11,
                    27.0}),
            "Virtual voice controls must be queueable as a UI batch");
    engine.synchronizePendingCommands();
    require(engineSession.virtualVoices[0].enabled
                && engineSession.virtualVoices[0].sourceIndex == 1
                && engineSession.virtualVoices[0].division == 4
                && engineSession.virtualVoices[0].pitchSemitones == -7
                && approximately(engineSession.virtualVoices[0].level, 0.42)
                && approximately(engineSession.virtualVoices[0].pan, 0.31)
                && engineSession.virtualVoices[0].patternLength == 12
                && approximately(engineSession.virtualVoices[0].focusStart, 0.2)
                && approximately(engineSession.virtualVoices[0].focusEnd, 0.85)
                && approximately(engineSession.virtualVoices[0].attackSeconds, 0.023)
                && approximately(engineSession.virtualVoices[0].releaseSeconds, 0.34)
                && engineSession.virtualVoices[0].pattern[11] == 27,
            "Virtual voice UI properties must persist in Project state");
    require(!engine.submitCommand({
                EngineCommandType::setVirtualVoiceProperty,
                0,
                static_cast<std::size_t>(VirtualVoiceProperty::level),
                0.81}),
            "Unsafe virtual voice level must be rejected");
    require(engine.submitCommand({EngineCommandType::setTempo, 2, 0, 137.0}),
            "A pending tempo update must be queueable before a host pause");
    engine.synchronizePendingCommands();
    require(approximately(engineSession.sequencer.tempo(), 137.0)
                && engineSession.sequencer.division() == 2,
            "A stopped host must be able to synchronize UI commands before snapshots");

    const auto requireTruePeakTolerance = [] (
        double measured, double expected, const char* message)
    {
        require(measured >= expected - 0.4 && measured <= expected + 0.2,
                message);
    };
    const auto ebuTruePeakFixtures =
        validation::makeEbuTruePeakFixtures();
    const auto fixtureFor = [&ebuTruePeakFixtures] (int caseNumber)
        -> const validation::TruePeakFixture&
    {
        const auto found = std::find_if(
            ebuTruePeakFixtures.begin(), ebuTruePeakFixtures.end(),
            [caseNumber] (const auto& fixture)
            {
                return fixture.caseNumber == caseNumber;
            });
        if (found == ebuTruePeakFixtures.end())
            throw std::logic_error("Missing EBU true-peak fixture");
        return *found;
    };
    for (const auto& fixture : ebuTruePeakFixtures)
    {
        requireTruePeakTolerance(
            measureTruePeak(fixture.samples), fixture.expectedDbtp,
            "EBU true-peak case 15-23 must remain within tolerance");
        if (!fixture.derivedTransient)
            continue;
        const auto limited = measureLimitedTruePeak(fixture.samples);
        const auto limitedDbtp = 20.0 * std::log10(limited.outputTruePeak);
        require(limited.ceilingEngaged
                    && limited.gainReductionDb > 0.5F
                    && limitedDbtp <= -1.0 + 0.1
                    && limitedDbtp >= -1.8,
                "Limiter must contain synthesized EBU transient case 20-23 near -1 dBTP");
    }

    LookaheadLimiter limiterContract;
    limiterContract.prepare(48000.0);
    require(limiterContract.latencySamples() == 240,
            "Five-millisecond lookahead must declare 240 samples at 48 kHz");
    for (const auto rate : std::array<double, 4> {
             44100.0, 48000.0, 96000.0, 192000.0})
    {
        LookaheadLimiter impulseLimiter;
        impulseLimiter.prepare(rate);
        static_cast<void>(impulseLimiter.process({4.0F, -2.0F}));
        for (std::size_t frame = 0;
             frame < impulseLimiter.latencySamples() + 128; ++frame)
            static_cast<void>(impulseLimiter.process({}));
        const auto impulseTelemetry = impulseLimiter.telemetry();
        require(impulseLimiter.latencySamples()
                    == static_cast<std::size_t>(
                        std::llround(rate * 0.005))
                    && impulseTelemetry.ceilingEngaged
                    && impulseTelemetry.gainReductionDb > 3.0F
                    && impulseTelemetry.outputTruePeak
                        <= std::pow(10.0F, -0.9F / 20.0F),
                "Impulse limiting and lookahead latency must scale across sample rates");
    }
    const auto transparentLimiter = measureLimitedTruePeak(
        fixtureFor(16).samples);
    requireTruePeakTolerance(
        20.0 * std::log10(transparentLimiter.outputTruePeak), -6.0,
        "Lookahead limiter must preserve a tone below its ceiling");
    require(transparentLimiter.gainReductionDb < 0.05F
                && !transparentLimiter.ceilingEngaged,
            "Lookahead limiter must remain neutral below its ceiling");
    const auto limitedEbuCase19 = measureLimitedTruePeak(
        fixtureFor(19).samples);
    const auto limitedCase19Dbtp = 20.0 * std::log10(
        limitedEbuCase19.outputTruePeak);
    require(limitedEbuCase19.ceilingEngaged
                && limitedEbuCase19.gainReductionDb > 3.0F
                && limitedCase19Dbtp <= -1.0 + 0.1
                && limitedCase19Dbtp >= -1.6,
            "Lookahead limiter must contain EBU case 19 near the -1 dBTP ceiling");

    OutputStage liveSafety;
    liveSafety.prepare(48000.0);
    const auto safetyCeiling = liveSafety.ceilingLinear();
    StereoSample linkedLimited;
    for (std::size_t frame = 0;
         frame < liveSafety.latencySamples() + 64; ++frame)
        linkedLimited = liveSafety.process({4.0F, -2.0F});
    require(std::abs(linkedLimited.left) <= safetyCeiling + 1.0e-6F
                && std::abs(linkedLimited.right) <= safetyCeiling + 1.0e-6F,
            "Live safety must enforce its linked stereo sample ceiling");
    require(std::abs(linkedLimited.right / linkedLimited.left + 0.5F) < 1.0e-5F,
            "Linked stereo limiting must preserve the instantaneous channel ratio");
    const auto guarded = liveSafety.process({
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()});
    require(std::isfinite(guarded.left) && std::isfinite(guarded.right)
                && liveSafety.telemetry().nonFiniteSamples == 2,
            "Live safety must contain and count non-finite samples");
    const auto safetyInterval = liveSafety.consumeTelemetry();
    require(safetyInterval.inputSamplePeak > 1.0F
                && safetyInterval.outputSamplePeak <= safetyCeiling + 1.0e-6F
                && safetyInterval.inputTruePeak > 0.0F
                && safetyInterval.outputTruePeak > 0.0F
                && safetyInterval.gainReductionDb > 0.0F
                && safetyInterval.sampleCeilingLatched
                && safetyInterval.nonFiniteSamples == 2,
            "Live safety telemetry must expose input/output peaks, GR and guards");
    require(liveSafety.consumeTelemetry().nonFiniteSamples == 0,
            "Live safety interval telemetry must be consumable without resetting DSP state");
    OutputStage dcSafety;
    dcSafety.prepare(48000.0);
    StereoSample dcOutput;
    for (std::size_t frame = 0; frame < 48000; ++frame)
        dcOutput = dcSafety.process({0.5F, -0.5F});
    require(std::abs(dcOutput.left) < 1.0e-3F
                && std::abs(dcOutput.right) < 1.0e-3F,
            "Live safety DC blocker must reject a sustained offset");
    require(liveSafety.latencySamples() == 240,
            "Live true-peak safety must declare its five-millisecond latency");
    liveSafety.prepare(96000.0);
    require(liveSafety.latencySamples() == 480
                && liveSafety.telemetry().nonFiniteSamples == 0,
            "Output safety re-prepare must reset state and scale latency");

    OutputStage technicalOutput;
    technicalOutput.prepare(48000.0);
    double fullLevelSquare = 0.0;
    double trimmedSquare = 0.0;
    std::size_t technicalFrame = 0;
    const auto processTechnicalTone = [&] (std::size_t frames,
                                           double& squareSum)
    {
        for (std::size_t frame = 0; frame < frames; ++frame, ++technicalFrame)
        {
            const auto sample = static_cast<float>(0.5 * std::sin(
                2.0 * 3.14159265358979323846 * 1000.0
                * static_cast<double>(technicalFrame) / 48000.0));
            const auto output = technicalOutput.process({sample, sample});
            if (frame >= frames / 2)
                squareSum += static_cast<double>(output.left) * output.left;
        }
    };
    processTechnicalTone(2400, fullLevelSquare);
    technicalOutput.setOutputTrimDb(-6.0F);
    processTechnicalTone(2400, trimmedSquare);
    const auto trimRmsRatio = std::sqrt(trimmedSquare / fullLevelSquare);
    require(std::abs(trimRmsRatio - std::pow(10.0, -6.0 / 20.0)) < 0.01
                && std::abs(technicalOutput.outputTrimDb() + 6.0F) < 1.0e-6F,
            "Technical output trim must be a smooth independent dB stage");
    technicalOutput.setMuted(true);
    float mutedTailPeak = 0.0F;
    double previousMuteSample = 0.0;
    double maximumMuteStep = 0.0;
    for (std::size_t frame = 0; frame < 1200; ++frame, ++technicalFrame)
    {
        const auto sample = static_cast<float>(0.5 * std::sin(
            2.0 * 3.14159265358979323846 * 1000.0
            * static_cast<double>(technicalFrame) / 48000.0));
        const auto output = technicalOutput.process({sample, sample});
        maximumMuteStep = std::max(
            maximumMuteStep,
            std::abs(static_cast<double>(output.left) - previousMuteSample));
        previousMuteSample = output.left;
        if (frame >= 1000)
            mutedTailPeak = std::max(mutedTailPeak, std::abs(output.left));
    }
    require(technicalOutput.isMuted() && mutedTailPeak < 1.0e-5F
                && maximumMuteStep < 0.1,
            "Technical mute must ramp to digital silence without a large step");
    technicalOutput.setMuted(false);
    float unmutedTailPeak = 0.0F;
    for (std::size_t frame = 0; frame < 1200; ++frame, ++technicalFrame)
    {
        const auto sample = static_cast<float>(0.5 * std::sin(
            2.0 * 3.14159265358979323846 * 1000.0
            * static_cast<double>(technicalFrame) / 48000.0));
        const auto output = technicalOutput.process({sample, sample});
        if (frame >= 1000)
            unmutedTailPeak = std::max(unmutedTailPeak, std::abs(output.left));
    }
    require(!technicalOutput.isMuted() && unmutedTailPeak > 0.20F,
            "Technical unmute must restore output through the ramp");

    SessionModel previewSafetySession;
    AudioEngine previewSafetyEngine(previewSafetySession);
    previewSafetyEngine.setOutputProfile(OutputProfile::liveSafe);
    require(previewSafetyEngine.setOutputTrimDb(-6.0F)
                && !previewSafetyEngine.setOutputTrimDb(0.1F)
                && !previewSafetyEngine.setOutputTrimDb(-24.1F),
            "Engine output trim must accept only the technical attenuation range");
    previewSafetyEngine.prepare(48000.0);
    require(previewSafetyEngine.submitCommand({EngineCommandType::startRecording}),
            "Post-safety preview recording must be queueable");
    require(previewSafetyEngine.outputLatencySamples() == 240,
            "Live engine must expose the output-stage latency");
    std::array<float, 1024> previewInputLeft;
    std::array<float, 1024> previewInputRight;
    std::array<float, 1024> previewOutputLeft {};
    std::array<float, 1024> previewOutputRight {};
    for (std::size_t frame = 0; frame < previewInputLeft.size(); ++frame)
    {
        const auto polarity = frame % 2 == 0 ? 1.0F : -1.0F;
        previewInputLeft[frame] = 2.0F * polarity;
        previewInputRight[frame] = -1.0F * polarity;
    }
    previewSafetyEngine.processBlock(
        previewOutputLeft.data(), previewOutputRight.data(),
        previewOutputLeft.size(),
        previewInputLeft.data(), previewInputRight.data());
    const auto previewMaximum = *std::max_element(
        previewOutputLeft.begin(), previewOutputLeft.end(),
        [] (float left, float right)
        {
            return std::abs(left) < std::abs(right);
        });
    require(std::abs(previewMaximum) <= safetyCeiling + 1.0e-6F,
            "Library Preview must pass through the live sample ceiling");
    const auto previewMeter = previewSafetyEngine.consumeOutputPeak();
    const auto previewSafety = previewSafetyEngine.consumeOutputSafetyTelemetry();
    require(std::abs(previewMeter.left) <= safetyCeiling + 1.0e-6F
                && previewMeter.left > 0.0F,
            "Post-safety meter must observe Library Preview");
    require(previewSafety.liveSafe && previewSafety.ceilingEngaged
                && previewSafety.inputSamplePeak > 1.0F
                && previewSafety.inputTruePeak > 0.0F
                && previewSafety.outputTruePeak > 0.0F
                && previewSafety.gainReductionDb > 0.0F
                && previewSafety.rms.left > 0.0F,
            "Post-safety telemetry must expose Preview RMS, input peak and GR");
    require(std::abs(previewSafety.outputTrimDb + 6.0F) < 1.0e-6F
                && !previewSafety.muted && !previewSafety.suspended,
            "Output telemetry must distinguish trim, mute and suspension");
    StereoSample previewRecordedFrame;
    std::size_t previewRecordedCount = 0;
    while (previewSafetyEngine.popRecordedFrame(previewRecordedFrame))
    {
        if (previewRecordedCount == previewSafetyEngine.outputLatencySamples() + 8)
            require(std::abs(previewRecordedFrame.left
                             - previewOutputLeft[previewRecordedCount]) < 1.0e-6F
                        && std::abs(previewRecordedFrame.right
                            - previewOutputRight[previewRecordedCount]) < 1.0e-6F,
                    "Post-safety recording must capture the same preview sent to output");
        ++previewRecordedCount;
    }
    require(previewRecordedCount == previewOutputLeft.size(),
            "Post-safety recording must capture every preview frame");

    previewSafetyEngine.setOutputMuted(true);
    previewSafetyEngine.processBlock(
        previewOutputLeft.data(), previewOutputRight.data(),
        previewOutputLeft.size(),
        previewInputLeft.data(), previewInputRight.data());
    const auto mutedSafety =
        previewSafetyEngine.consumeOutputSafetyTelemetry();
    require(mutedSafety.muted && !mutedSafety.suspended
                && std::abs(previewOutputLeft.back()) < 1.0e-5F,
            "Engine mute request must reach the audio thread and silence output");
    previewSafetyEngine.setOutputMuted(false);
    previewSafetyEngine.suspendOutput();
    previewSafetyEngine.processBlock(
        previewOutputLeft.data(), previewOutputRight.data(),
        previewOutputLeft.size(),
        previewInputLeft.data(), previewInputRight.data());
    const auto suspendedSafety =
        previewSafetyEngine.consumeOutputSafetyTelemetry();
    require(suspendedSafety.muted && suspendedSafety.suspended
                && std::abs(previewOutputLeft.back()) < 1.0e-5F,
            "Suspended device state must keep live output silent");
    previewSafetyEngine.prepare(96000.0);
    previewSafetyEngine.resumeOutput();
    std::array<float, 2048> resumedInput;
    std::array<float, 2048> resumedOutputLeft {};
    std::array<float, 2048> resumedOutputRight {};
    resumedInput.fill(0.25F);
    previewSafetyEngine.processBlock(
        resumedOutputLeft.data(), resumedOutputRight.data(),
        resumedOutputLeft.size(), resumedInput.data(), resumedInput.data());
    const auto resumedSafety =
        previewSafetyEngine.consumeOutputSafetyTelemetry();
    require(!resumedSafety.muted && !resumedSafety.suspended
                && resumedOutputLeft.front() == 0.0F
                && resumedOutputLeft.back() > 0.0F,
            "Reconnect prepare must resume from silence through a safe fade-in");

    SessionModel worstCaseSession;
    worstCaseSession.masterLevel = 1.0;
    worstCaseSession.mixer.sourceA.level = 1.25;
    worstCaseSession.mixer.sourceB.level = 1.25;
    worstCaseSession.patterns.setCell(0, 0, 0);
    worstCaseSession.patterns.setCell(0, 1, 128);
    for (std::size_t voice = 0;
         voice < worstCaseSession.virtualVoices.size(); ++voice)
    {
        auto& state = worstCaseSession.virtualVoices[voice];
        state.enabled = true;
        state.sourceIndex = voice;
        state.division = 1;
        state.level = 1.0;
        state.attackSeconds = 0.001;
        state.releaseSeconds = 1.0;
    }
    std::vector<float> worstCaseSamples(48000, 0.95F);
    StereoAudioBuffer worstCaseBuffer(
        48000.0, worstCaseSamples, worstCaseSamples);
    AudioEngine worstCaseEngine(worstCaseSession);
    worstCaseEngine.setOutputProfile(OutputProfile::liveSafe);
    worstCaseEngine.prepare(48000.0);
    worstCaseEngine.setSourceBuffer(0, &worstCaseBuffer);
    worstCaseEngine.setSourceBuffer(1, &worstCaseBuffer);
    require(worstCaseEngine.submitCommand({EngineCommandType::start}),
            "Worst-case live transport must be queueable");
    constexpr std::size_t worstCaseFrames = 24000;
    std::vector<float> worstCaseExternal(worstCaseFrames, 0.5F);
    std::vector<float> worstCaseOutputLeft(worstCaseFrames, 0.0F);
    std::vector<float> worstCaseOutputRight(worstCaseFrames, 0.0F);
    worstCaseEngine.processBlock(
        worstCaseOutputLeft.data(), worstCaseOutputRight.data(),
        worstCaseFrames,
        worstCaseExternal.data(), worstCaseExternal.data());
    const auto worstCaseTelemetry =
        worstCaseEngine.consumeOutputSafetyTelemetry();
    require(worstCaseTelemetry.inputSamplePeak > 1.5F
                && worstCaseTelemetry.gainReductionDb > 0.0F
                && worstCaseTelemetry.ceilingEngaged,
            "In-phase sources, virtual voices and Preview must reach live safety");
    require(worstCaseTelemetry.outputTruePeak
                    <= std::pow(10.0F, -0.9F / 20.0F)
                && std::all_of(
                    worstCaseOutputLeft.begin(), worstCaseOutputLeft.end(),
                    [safetyCeiling] (float sample)
                    {
                        return std::isfinite(sample)
                            && std::abs(sample) <= safetyCeiling + 1.0e-6F;
                    })
                && std::all_of(
                    worstCaseOutputRight.begin(), worstCaseOutputRight.end(),
                    [safetyCeiling] (float sample)
                    {
                        return std::isfinite(sample)
                            && std::abs(sample) <= safetyCeiling + 1.0e-6F;
                    }),
            "Worst-case live sum must remain finite and below the true-peak ceiling");

    SessionModel offlineSession;
    offlineSession.patterns.setCell(0, 0, 0);
    StereoAudioBuffer offlineBuffer(
        800.0, std::vector<float>(800, 0.25F), std::vector<float>(800, -0.5F));
    AudioEngine offlineEngine(offlineSession);
    offlineEngine.prepare(800.0);
    offlineEngine.setSourceBuffer(0, &offlineBuffer);
    require(offlineEngine.submitCommand({EngineCommandType::start}),
            "Offline transport start must be queueable");
    const auto offlineRender = renderOffline(offlineEngine, 128, 31);
    require(offlineRender.left.size() == 128 && offlineRender.peak > 0.0F,
            "Offline rendering must use the real engine and produce measurements");
    require(offlineRender.checksum != 0 && offlineRender.meanSquare > 0.0,
            "Offline rendering must produce a deterministic checksum and energy");
    const auto measuredPeak = offlineEngine.consumeOutputPeak();
    require(measuredPeak.left > 0.0F && measuredPeak.right > 0.0F,
            "Post-MASTER meters must receive the rendered stereo peak");
    const auto consumedPeak = offlineEngine.consumeOutputPeak();
    require(approximately(consumedPeak.left, 0.0)
                && approximately(consumedPeak.right, 0.0),
            "Meter peaks must be atomically consumed by the UI");

    SessionModel repeatSession;
    repeatSession.patterns.setCell(0, 0, 0);
    AudioEngine repeatEngine(repeatSession);
    repeatEngine.prepare(800.0);
    repeatEngine.setSourceBuffer(0, &offlineBuffer);
    require(repeatEngine.submitCommand({EngineCommandType::start}),
            "Repeated offline transport start must be queueable");
    const auto repeatedRender = renderOffline(repeatEngine, 128, 64);
    require(repeatedRender.checksum == offlineRender.checksum
                && approximately(repeatedRender.meanSquare, offlineRender.meanSquare),
            "Golden renders must not depend on the host audio block size");

    rejected = false;
    try
    {
        static_cast<void>(renderOffline(offlineEngine, maxOfflineSamples + 1));
    }
    catch (const std::length_error&)
    {
        rejected = true;
    }
    require(rejected, "Offline rendering must enforce its memory safety ceiling");

    StereoAudioBuffer comparisonReference(
        48000.0, {0.5F, -0.5F}, {0.25F, -0.25F});
    StereoAudioBuffer comparisonExact(
        48000.0, {0.5F, -0.5F}, {0.25F, -0.25F});
    const auto exactComparison =
        compareAudio(comparisonReference, comparisonExact);
    require(approximately(exactComparison.differenceRms, 0.0)
                && approximately(exactComparison.correlation, 1.0)
                && std::isinf(exactComparison.signalToNoiseDb),
            "Identical golden WAVs must report zero difference and infinite SNR");

    StereoAudioBuffer comparisonScaled(
        48000.0, {0.25F, -0.25F}, {0.125F, -0.125F});
    const auto scaledComparison =
        compareAudio(comparisonReference, comparisonScaled);
    require(scaledComparison.frames == 2
                && scaledComparison.maximumAbsoluteDifference == 0.25
                && approximately(scaledComparison.correlation, 1.0)
                && approximately(scaledComparison.signalToNoiseDb, 6.020599913279624),
            "WAV comparison metrics must expose gain-only differences");

    rejected = false;
    try
    {
        StereoAudioBuffer wrongRate(44100.0, {0.0F}, {0.0F});
        static_cast<void>(compareAudio(comparisonReference, wrongRate));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "WAV comparison must reject unaligned inputs");

    StereoAudioBuffer delayedCandidate(
        48000.0, {0.0F, 0.5F, -0.5F}, {0.0F, 0.25F, -0.25F});
    const auto alignedComparison =
        compareAudioAligned(comparisonReference, delayedCandidate, 1);
    require(alignedComparison.frames == comparisonReference.size()
                && approximately(alignedComparison.differenceRms, 0.0)
                && std::isinf(alignedComparison.signalToNoiseDb),
            "Known candidate latency must be removable without copying audio");

    SessionModel voiceSession;
    voiceSession.patterns.setCell(0, 0, gapCellCode);
    voiceSession.virtualVoices[0].enabled = true;
    voiceSession.virtualVoices[0].division = 1;
    voiceSession.virtualVoices[0].level = 0.5;
    voiceSession.virtualVoices[0].pan = -1.0;
    voiceSession.virtualVoices[0].attackSeconds = 0.001;
    voiceSession.virtualVoices[0].releaseSeconds = 0.005;
    AudioEngine voiceEngine(voiceSession);
    voiceEngine.prepare(800.0);
    voiceEngine.setSourceBuffer(0, &offlineBuffer);
    require(voiceEngine.submitCommand({EngineCommandType::start}),
            "Virtual voice transport must be queueable");
    const auto voiceRender = renderOffline(voiceEngine, 80);
    require(voiceRender.peak > 0.0F,
            "An enabled virtual voice must render even when the main pattern cell is GAP");
    require(std::all_of(voiceRender.right.begin(), voiceRender.right.end(),
                        [] (float sample) { return approximately(sample, 0.0); }),
            "Full-left virtual voice pan must silence its right output");

    HeritagePitch heritage;
    heritage.prepare(48000.0);
    auto heritageSample = heritage.process({0.25F, -0.5F});
    require(approximately(heritageSample.left, 0.25)
                && approximately(heritageSample.right, -0.5),
            "Heritage Pitch bypass mode must preserve the dry signal");
    heritage.setSemitones(7);
    heritage.setMode(1.0F);
    double heritageEnergy = 0.0;
    for (std::size_t sample = 0; sample < 2000; ++sample)
    {
        heritageSample = heritage.process({sample == 0 ? 1.0F : 0.0F, 0.0F});
        heritageEnergy += static_cast<double>(heritageSample.left) * heritageSample.left;
    }
    require(std::isfinite(heritageSample.left) && std::isfinite(heritageSample.right),
            "Legacy modulated delay pitch must remain numerically stable");
    require(heritageEnergy > 0.0 && heritageEnergy < 4.0,
            "Legacy pitch impulse energy must remain finite and bounded");
    require(voiceEngine.submitCommand(
                {EngineCommandType::setHeritagePitch, 0, 0, -5.0, 1.0}),
            "Heritage Pitch must be controllable through the realtime queue");
    require(voiceEngine.submitCommand({
                EngineCommandType::setVirtualVoiceProperty,
                0,
                static_cast<std::size_t>(VirtualVoiceProperty::pitch),
                -7.0}),
            "Virtual voice properties must be controllable through the realtime queue");
    require(voiceEngine.submitCommand(
                {EngineCommandType::setVirtualVoicePatternCell, 0, 3, 12.0}),
            "Virtual voice pattern cells must be controllable through the realtime queue");
    std::array<float, 1> voiceCommandLeft {};
    std::array<float, 1> voiceCommandRight {};
    voiceEngine.processBlock(voiceCommandLeft.data(), voiceCommandRight.data(), 1);
    require(voiceSession.virtualVoices[0].pitchSemitones == -7
                && voiceSession.virtualVoices[0].pattern[3] == 12,
            "Queued virtual voice edits must update audio-thread state");

    SessionModel projectSession;
    projectSession.selectSource(1);
    projectSession.patterns.setCell(3, 4, 205);
    projectSession.sources[0].sliceBank.divideRegion(0.1, 0.9, 16);
    require(projectSession.captureFormSliceBank(
                0, SliceBankProfile::manual)
                == SliceBankProfile::manual,
            "Project fixture must capture a named FORM bank");
    projectSession.mixer.sourceA.pan = -0.4;
    projectSession.masterLevel = 0.63;
    projectSession.heritagePitchSemitones = 5;
    projectSession.sequencer.setTempo(137.0, 2);
    projectSession.sequencer.setTiming(TimingMode::jitter, 23.0, 0xabcdef01U);
    projectSession.sequencer.selectPattern(3);
    projectSession.patternMemory[3][2] = true;
    projectSession.patternMemory[3][6] = true;
    projectSession.patternTransform = {
        true, 3, transformBase, {73, 41, 66}
    };
    projectSession.formDirector.setEnabled(true);
    static_cast<void>(projectSession.formDirector.selectScene(3));
    auto projectFormScene = projectSession.formDirector.state().scenes[3];
    projectFormScene.name = makeFormText("PEAK METAL");
    static_cast<void>(projectSession.formDirector.replaceCurrentScene(
        projectFormScene, true));
    static_cast<void>(projectSession.formDirector.notePhraseCompleted());
    require(projectSession.controlTrace.append(0, 120, 0)
                && projectSession.controlTrace.append(120, 144, 5),
            "Project TRACE fixture must contain two control points");
    projectSession.assistedRng.setSeed(0x10203040U);
    projectSession.assisted = assistedSettings;
    projectSession.motifLocks = {
        true, false, true, false, true, false, true, false};
    static_cast<void>(projectSession.assistedRng.next());
    static_cast<void>(projectSession.assistedRng.next());
    auto savedProject = captureProjectState(projectSession);
    savedProject.sourceReferences[0] = {
        "source-a.wav", "source-a.wav", 123456, 1720000000000ULL, "audio/wav"};
    savedProject.sourceReferences[1] = {
        "source-b.wav", "audio/source-b.wav", 654321, 1720000001000ULL, "audio/wav"};
    savedProject.selectedMotifSlot = 3;
    auto& savedMotif = savedProject.motifSlots[3];
    savedMotif.occupied = true;
    savedMotif.name = "RUPTURE A";
    savedMotif.capturedAt = "2026-07-30T23:59:00Z";
    savedMotif.sources = projectSession.sources;
    savedMotif.pattern = projectSession.patterns.pattern(3);
    savedMotif.cellMemory = projectSession.patternMemory[3];
    savedMotif.virtualVoices = projectSession.virtualVoices;
    savedMotif.mixer = projectSession.mixer;
    savedMotif.activeSource = 1;
    savedMotif.currentPattern = 3;
    savedMotif.bpm = 137.0;
    savedMotif.divisionMode = 2;
    savedMotif.timingMode = TimingMode::jitter;
    savedMotif.jitter = 23.0;
    savedMotif.heritagePitchSemitones = 5;
    savedMotif.heritagePitchMode = 1.0;
    savedProject.hasAlbumProject = true;
    savedProject.albumProject.title = "Ruptures in water";
    savedProject.albumProject.artist = "RASGO";
    savedProject.albumProject.notes = "Editorial order travels with Project v2.";
    navalha::AlbumProjectTrack savedAlbumTrack;
    savedAlbumTrack.id = "album-track-1";
    savedAlbumTrack.takeId = "take-1";
    savedAlbumTrack.title = "Opening cut";
    savedAlbumTrack.filename = "opening.wav";
    savedAlbumTrack.status = "MASTER";
    savedAlbumTrack.notes = "Keep the abrupt ending.";
    savedAlbumTrack.durationSeconds = 12.5;
    savedAlbumTrack.settings.durationSeconds = 12.5;
    savedAlbumTrack.settings.trimDb = -1.5;
    savedAlbumTrack.settings.gapAfterSeconds = 0.5;
    savedAlbumTrack.settings.fadeInSeconds = 0.25;
    savedAlbumTrack.settings.fadeOutSeconds = 0.1;
    savedAlbumTrack.hasAnalysis = true;
    savedAlbumTrack.review.status = "MASTER";
    savedAlbumTrack.review.rating = 4;
    savedAlbumTrack.review.notes = "ready";
    savedAlbumTrack.recipeJson = "{\"format\":\"navalha-take-recipe\"}";
    savedProject.albumProject.tracks.push_back(std::move(savedAlbumTrack));

    SessionModel restoredSession;
    restoreProjectState(savedProject, restoredSession);
    require(restoredSession.activeSource == 1
                && restoredSession.sources[0].sliceBank.size() == 16
                && restoredSession.patterns.cell(3, 4) == 205,
            "Project v2 restore must preserve sources, slices and A/B pattern codes");
    require(approximately(restoredSession.sequencer.tempo(), 137.0)
                && restoredSession.sequencer.division() == 2
                && restoredSession.sequencer.timing() == TimingMode::jitter
                && restoredSession.sequencer.seed() == 0xabcdef01U,
            "Project v2 restore must preserve deterministic sequencer state");
    require(approximately(restoredSession.masterLevel, 0.63)
                && restoredSession.heritagePitchSemitones == 5,
            "Project v2 restore must preserve DSP state");
    require(restoredSession.patternMemory[3][2]
                && restoredSession.patternMemory[3][6]
                && restoredSession.patternTransform.hasBase
                && restoredSession.patternTransform.patternIndex == 3
                && restoredSession.patternTransform.base == transformBase
                && restoredSession.patternTransform.amounts.mutation == 73
                && restoredSession.patternTransform.amounts.erosion == 41
                && restoredSession.patternTransform.amounts.deconstruct == 66
                && restoredSession.formDirector.state().currentScene == 3
                && restoredSession.formDirector.state().bar == 1
                && restoredSession.controlTrace.size() == 2,
            "Project v2 restore must preserve MEMORY and reversible transforms");
    require(restoredSession.assistedRng.seed() == 0x10203040U
                && restoredSession.assistedRng.state() == projectSession.assistedRng.state()
                && restoredSession.assistedRng.cursor() == 2
                && !restoredSession.assisted.enabled
                && restoredSession.assisted.repeat
                && restoredSession.motifLocks.source
                && restoredSession.motifLocks.pattern
                && restoredSession.motifLocks.pitch
                && restoredSession.motifLocks.mix
                && approximately(restoredSession.assistedRng.next(),
                                 projectSession.assistedRng.next()),
            "Project restore must preserve Assisted settings/locks but reopen disarmed");

    const auto projectJson = encodeProjectJson(savedProject);
    const auto decodedProject = decodeProjectJson(projectJson);
    require(decodedProject.activeSource == savedProject.activeSource
                && decodedProject.sources[0].sliceBank.size() == 16
                && decodedProject.formSliceBanks[0].has(
                    SliceBankProfile::manual)
                && decodedProject.formSliceBanks[0].bank(
                    SliceBankProfile::manual).size() == 16
                && decodedProject.patterns.cell(3, 4) == 205
                && approximately(decodedProject.bpm, 137.0)
                && decodedProject.assistedSeed == 0x10203040U
                && decodedProject.assistedCursor == 2
                && decodedProject.sourceReferences[0].filename == "source-a.wav"
                && decodedProject.sourceReferences[0].size == 123456
                && decodedProject.sourceReferences[1].relativePath
                    == "audio/source-b.wav"
                && decodedProject.sourceReferences[1].lastModified
                    == 1720000001000ULL
                && decodedProject.patternMemory[3][2]
                && decodedProject.patternMemory[3][6]
                && decodedProject.patternTransform.hasBase
                && decodedProject.patternTransform.base == transformBase
                && decodedProject.patternTransform.amounts.deconstruct == 66
                && decodedProject.formDirector.currentScene == 3
                && formText(decodedProject.formDirector.scenes[3].name)
                    == "PEAK METAL"
                && decodedProject.formDirector.bar == 1
                && !decodedProject.formDirector.enabled
                && decodedProject.controlTrace.size() == 2
                && decodedProject.controlTrace.points()[1]
                    == ControlTracePoint {120, 144, 5}
                && decodedProject.assisted.enabled
                && decodedProject.assisted.repeat
                && decodedProject.assisted.useGaps
                && decodedProject.motifLocks.source
                && decodedProject.motifLocks.pattern
                && decodedProject.motifLocks.pitch
                && decodedProject.motifLocks.mix
                && decodedProject.selectedMotifSlot == 3
                && decodedProject.motifSlots[3].occupied
                && decodedProject.motifSlots[3].name == "RUPTURE A"
                && decodedProject.motifSlots[3].currentPattern == 3
                && decodedProject.motifSlots[3].pattern[4] == 205
                && decodedProject.motifSlots[3].cellMemory[2]
                && decodedProject.motifSlots[3].activeSource == 1
                && decodedProject.motifSlots[3].sources[0].sliceBank.size() == 16
                && decodedProject.motifSlots[3].timingMode
                    == TimingMode::jitter
                && decodedProject.motifSlots[3].heritagePitchSemitones == 5
                && decodedProject.hasAlbumProject
                && decodedProject.albumProject.title == "Ruptures in water"
                && decodedProject.albumProject.tracks.size() == 1
                && decodedProject.albumProject.tracks[0].takeId == "take-1"
                && decodedProject.albumProject.tracks[0].settings.trimDb == -1.5
                && decodedProject.assisted.minBpm == 72
                && decodedProject.assisted.maxBpm == 144
                && decodedProject.assisted.variation == 68,
            "Project v2 JSON must round-trip musical and deterministic state");
    require(projectJson.find("\"format\":\"navalha-project\"") != std::string::npos
                && projectJson.find("\"version\":2") != std::string::npos
                && projectJson.find("\"RUPTURE A\"") != std::string::npos
                && projectJson.find("\"Ruptures in water\"") != std::string::npos,
            "Project JSON must retain the v0.28.1 format/version contract");

    ProjectStateV1 legacyProject;
    legacyProject.sourceA.sliceBank.divideRegion(0.2, 0.8, 4);
    legacyProject.activeSource = 1;
    legacyProject.bpm = 999.0;
    const auto migratedProject = migrateProjectV1(legacyProject);
    require(migratedProject.sources[0].sliceBank.size() == 4
                && migratedProject.sources[1].sliceBank.size() == 8,
            "Project v1 migration must preserve A and create a fresh default B");
    require(migratedProject.activeSource == 0 && approximately(migratedProject.bpm, 400.0),
            "Project v1 migration must clamp legacy state and ignore absent dual material");

    RecordingFifo<4> recordingQueue;
    require(recordingQueue.push({1.0F, -1.0F})
                && recordingQueue.push({2.0F, -2.0F})
                && recordingQueue.push({3.0F, -3.0F}),
            "Recording FIFO capacity N must accept N-1 frames");
    require(!recordingQueue.push({4.0F, -4.0F}) && recordingQueue.dropped() == 1,
            "A slow writer must drop and count frames without blocking audio");
    StereoSample recordedFrame;
    require(recordingQueue.pop(recordedFrame)
                && approximately(recordedFrame.left, 1.0),
            "Recording FIFO must preserve frame order");

    require(offlineEngine.submitCommand({EngineCommandType::startRecording}),
            "Recording start must be queueable");
    std::array<float, 32> recordingLeft {};
    std::array<float, 32> recordingRight {};
    offlineEngine.processBlock(recordingLeft.data(), recordingRight.data(), recordingLeft.size());
    require(offlineEngine.submitCommand({EngineCommandType::stopRecording}),
            "Recording stop must be queueable");
    offlineEngine.processBlock(recordingLeft.data(), recordingRight.data(), 1);
    std::size_t recordedCount = 0;
    while (offlineEngine.popRecordedFrame(recordedFrame))
        ++recordedCount;
    require(recordedCount == 32,
            "Recorder FIFO must capture the real post-MASTER output only while armed");

    const auto recordingPath = std::filesystem::temp_directory_path()
        / ("navalha-juce-writer-"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
           + ".wav");
    RecordingWriterService recordingService(offlineEngine);
    require(recordingService.start(recordingPath, 800, WavSampleFormat::pcm24),
            "Background recording writer must open a new WAV");
    offlineEngine.processBlock(recordingLeft.data(), recordingRight.data(), recordingLeft.size());
    std::thread recordingStopper([&recordingService] { recordingService.stop(); });
    for (std::size_t attempt = 0; attempt < 500 && recordingService.isRunning(); ++attempt)
    {
        offlineEngine.processBlock(recordingLeft.data(), recordingRight.data(), 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    recordingStopper.join();
    require(recordingService.framesWritten() >= recordingLeft.size()
                && recordingService.framesWritten() < recordingLeft.size() + 32
                && recordingService.error().empty()
                && std::filesystem::file_size(recordingPath) > 44,
            "Background writer must await audio acknowledgement and drain the FIFO");
    const auto partialPrefix = "." + recordingPath.filename().string() + ".";
    const auto hasRecordingPartial = std::any_of(
        std::filesystem::directory_iterator(recordingPath.parent_path()),
        std::filesystem::directory_iterator {},
        [&] (const std::filesystem::directory_entry& entry)
        {
            const auto name = entry.path().filename().string();
            return name.starts_with(partialPrefix) && name.ends_with(".partial");
        });
    require(!hasRecordingPartial,
            "Successful recording must atomically publish and remove its partial file");
    require(std::filesystem::remove(recordingPath),
            "The tiny writer validation WAV must be removed immediately");

    const auto limitedRecordingPath = recordingPath.parent_path()
        / (recordingPath.stem().string() + "-limited.wav");
    RecordingWriterService limitedRecording(offlineEngine);
    require(limitedRecording.start(
                limitedRecordingPath, 800, WavSampleFormat::pcm24, {}, 8),
            "A recording with a hard frame limit must start");
    offlineEngine.processBlock(
        recordingLeft.data(), recordingRight.data(), recordingLeft.size());
    for (std::size_t attempt = 0;
         attempt < 400 && limitedRecording.isRunning();
         ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    limitedRecording.stop();
    offlineEngine.processBlock(recordingLeft.data(), recordingRight.data(), 1);
    require(limitedRecording.framesWritten() == 8
                && limitedRecording.error() == "Recording safety limit reached"
                && !std::filesystem::exists(limitedRecordingPath),
            "Recording safety limit must stop disk writes and withhold a partial WAV");

    for (const auto format : {
             WavSampleFormat::pcm16, WavSampleFormat::pcm24, WavSampleFormat::float32})
    {
        std::ostringstream wav(std::ios::binary);
        WavStreamWriter writer(wav, 48000, format);
        writer.writeFrame({-1.0F, 1.0F});
        writer.writeFrame({0.25F, -0.25F});
        writer.finalize();
        const auto bytes = wav.str();
        const auto bytesPerSample = format == WavSampleFormat::pcm16 ? 2U
            : format == WavSampleFormat::pcm24 ? 3U : 4U;
        require(bytes.substr(0, 4) == "RIFF" && bytes.substr(8, 4) == "WAVE",
                "WAV writer must produce a RIFF/WAVE header");
        require(readU32(bytes, 40) == 2U * 2U * bytesPerSample
                    && bytes.size() == 44U + readU32(bytes, 40),
                "WAV header sizes must match the interleaved stereo payload");
        require(writer.framesWritten() == 2 && writer.isFinalized(),
                "WAV writer must report finalized frame count");
    }

    std::ostringstream encodedWav(std::ios::binary);
    WavStreamWriter encodedWriter(encodedWav, 48000, WavSampleFormat::pcm16);
    encodedWriter.writeFrame({-1.0F, 1.0F});
    encodedWriter.writeFrame({0.25F, -0.25F});
    encodedWriter.finalize();
    const auto encodedBytes = encodedWav.str();
    const auto decodedWav = decodeWav({
        reinterpret_cast<const std::uint8_t*>(encodedBytes.data()), encodedBytes.size()});
    require(decodedWav->size() == 2 && approximately(decodedWav->sampleRate(), 48000.0),
            "WAV decoder must recover frame count and sample rate");
    require(decodedWav->interpolated(0.0).left < -0.99F
                && decodedWav->interpolated(0.0).right > 0.99F,
            "WAV decoder must recover interleaved stereo PCM");

    constexpr std::size_t ditherFrames = 65536;
    const auto renderSilence = [] (WavSampleFormat format,
                                   WavEncodingOptions options)
    {
        std::ostringstream output(std::ios::binary);
        WavStreamWriter writer(output, 48000, format, {}, options);
        for (std::size_t frame = 0; frame < ditherFrames; ++frame)
            writer.writeFrame({});
        writer.finalize();
        return output.str();
    };
    const auto dithered16A = renderSilence(
        WavSampleFormat::pcm16, {WavDitherMode::tpdf, 0x12345678U});
    const auto dithered16B = renderSilence(
        WavSampleFormat::pcm16, {WavDitherMode::tpdf, 0x12345678U});
    const auto dithered16OtherSeed = renderSilence(
        WavSampleFormat::pcm16, {WavDitherMode::tpdf, 0x87654321U});
    const auto undithered16 = renderSilence(
        WavSampleFormat::pcm16, {WavDitherMode::none});
    require(dithered16A == dithered16B,
            "TPDF dither must be deterministic for an explicit seed");
    require(dithered16A != dithered16OtherSeed,
            "Different TPDF seeds must produce different integer payloads");
    require(std::all_of(
                undithered16.begin() + 44, undithered16.end(),
                [] (char byte) { return byte == 0; }),
            "Explicitly disabled dither must encode digital silence as zero");
    std::size_t nonZeroDitherSamples = 0;
    std::int64_t ditherSum = 0;
    for (std::size_t offset = 44; offset < dithered16A.size(); offset += 2)
    {
        const auto value = readI16(dithered16A, offset);
        require(value >= -1 && value <= 1,
                "TPDF silence must stay within one integer LSB");
        nonZeroDitherSamples += value != 0;
        ditherSum += value;
    }
    const auto ditherSampleCount = ditherFrames * 2;
    const auto nonZeroRatio = static_cast<double>(nonZeroDitherSamples)
        / static_cast<double>(ditherSampleCount);
    const auto ditherMean = static_cast<double>(ditherSum)
        / static_cast<double>(ditherSampleCount);
    require(nonZeroRatio > 0.23 && nonZeroRatio < 0.27,
            "TPDF silence must have the expected triangular one-LSB activity");
    require(std::abs(ditherMean) < 0.01,
            "TPDF dither must remain effectively zero-mean");

    const auto dithered24 = renderSilence(
        WavSampleFormat::pcm24, {WavDitherMode::tpdf, 0x12345678U});
    require(std::any_of(
                dithered24.begin() + 44, dithered24.end(),
                [] (char byte) { return byte != 0; }),
            "PCM24 must receive TPDF dither before fixed-point quantization");
    const auto floatSilence = renderSilence(
        WavSampleFormat::float32, {WavDitherMode::tpdf, 0x12345678U});
    require(std::all_of(
                floatSilence.begin() + 44, floatSilence.end(),
                [] (char byte) { return byte == 0; }),
            "Float32 export must never receive integer dither");
    std::ostringstream nonFiniteWav(std::ios::binary);
    WavStreamWriter nonFiniteWriter(
        nonFiniteWav, 48000, WavSampleFormat::float32);
    nonFiniteWriter.writeFrame({
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()});
    nonFiniteWriter.finalize();
    const auto nonFiniteBytes = nonFiniteWav.str();
    const auto sanitizedWav = decodeWav({
        reinterpret_cast<const std::uint8_t*>(nonFiniteBytes.data()),
        nonFiniteBytes.size()});
    const auto sanitizedFrame = sanitizedWav->interpolated(0.0);
    require(sanitizedFrame.left == 0.0F && sanitizedFrame.right == 0.0F,
            "WAV writer must prevent non-finite samples in every output format");

    const std::vector<std::uint8_t> extensiblePcm24 {
        'R','I','F','F', 66,0,0,0, 'W','A','V','E',
        'f','m','t',' ', 40,0,0,0,
        0xfe,0xff, 2,0, 0x80,0xbb,0,0, 0,0x65,4,0,
        6,0, 24,0, 22,0, 24,0, 3,0,0,0,
        1,0,0,0, 0,0,0x10,0, 0x80,0,0,0xaa, 0,0x38,0x9b,0x71,
        'd','a','t','a', 6,0,0,0,
        0,0,0x40, 0,0,0xc0
    };
    const auto decodedExtensible = decodeWav(extensiblePcm24);
    const auto extensibleFrame = decodedExtensible->interpolated(0.0);
    require(decodedExtensible->size() == 1
                && decodedExtensible->sampleRate() == 48000.0
                && approximately(extensibleFrame.left, 0.5)
                && approximately(extensibleFrame.right, -0.5),
            "WAVE_FORMAT_EXTENSIBLE PCM24 must decode like standard PCM24");

    rejected = false;
    try
    {
        static_cast<void>(decodeWav({
            reinterpret_cast<const std::uint8_t*>(encodedBytes.data()), encodedBytes.size()}, 1));
    }
    catch (const std::length_error&)
    {
        rejected = true;
    }
    require(rejected, "WAV decoder must enforce its frame safety ceiling");

    std::ostringstream metadataWav(std::ios::binary);
    WavStreamWriter metadataWriter(
        metadataWav,
        48000,
        WavSampleFormat::float32,
        {"Navalha take", "Navalha 2", "Migration validation", "2026", "RIFF metadata"});
    metadataWriter.writeFrame({0.1F, -0.1F});
    metadataWriter.finalize();
    const auto metadataBytes = metadataWav.str();
    require(metadataBytes.find("LIST") != std::string::npos
                && metadataBytes.find("INAM") != std::string::npos
                && metadataBytes.find("Navalha take") != std::string::npos
                && readU32(metadataBytes, 4) == metadataBytes.size() - 8,
            "WAV metadata must be stored in a correctly sized RIFF LIST/INFO chunk");
    const auto metadataDecoded = decodeWav({
        reinterpret_cast<const std::uint8_t*>(metadataBytes.data()), metadataBytes.size()});
    require(metadataDecoded->size() == 1,
            "WAV decoder must skip metadata chunks and still find audio data");

    const auto audioPayload = [] (const std::string& wav)
    {
        const auto dataOffset = findRiffChunk(wav, "data");
        require(dataOffset != std::string::npos && dataOffset + 8 <= wav.size(),
                "WAV fixture must contain a data chunk");
        const auto dataBytes = readU32(wav, dataOffset + 4);
        require(dataOffset + 8 + dataBytes <= wav.size(),
                "WAV fixture data chunk must fit its container");
        return wav.substr(dataOffset + 8, dataBytes);
    };
    const auto originalAudioPayload = audioPayload(metadataBytes);
    const auto metadataWithTrailing = metadataBytes + "TRAILING-BYTES";
    std::istringstream metadataInput(metadataWithTrailing, std::ios::binary);
    std::ostringstream rewrittenMetadata(std::ios::binary);
    const auto rewriteReport = rewriteWavInfoMetadata(
        metadataInput, rewrittenMetadata,
        {"Edited take", "Lúcio Araújo", "Album", "2027", "Reviewed"});
    const auto rewrittenMetadataBytes = rewrittenMetadata.str();
    require(rewriteReport.infoListsRemoved == 1
                && rewriteReport.infoListWritten
                && rewriteReport.audioDataBytes == originalAudioPayload.size()
                && rewrittenMetadataBytes.find("Edited take")
                    != std::string::npos
                && rewrittenMetadataBytes.find("Navalha take")
                    == std::string::npos,
            "RIFF metadata rewrite must replace the previous INFO list");
    require(audioPayload(rewrittenMetadataBytes) == originalAudioPayload,
            "RIFF metadata rewrite must preserve audio bytes exactly");
    require(rewrittenMetadataBytes.ends_with("TRAILING-BYTES")
                && readU32(rewrittenMetadataBytes, 4)
                    == rewriteReport.riffBytes - 8,
            "RIFF metadata rewrite must preserve bytes outside declared RIFF");

    std::istringstream secondMetadataInput(
        rewrittenMetadataBytes, std::ios::binary);
    std::ostringstream secondMetadataOutput(std::ios::binary);
    const auto secondRewriteReport = rewriteWavInfoMetadata(
        secondMetadataInput, secondMetadataOutput,
        {"Final title", "Artist", "Album", "2028", "Approved"});
    const auto secondMetadataBytes = secondMetadataOutput.str();
    const auto firstList = secondMetadataBytes.find("LIST");
    require(secondRewriteReport.infoListsRemoved == 1
                && firstList != std::string::npos
                && secondMetadataBytes.find("LIST", firstList + 4)
                    == std::string::npos
                && audioPayload(secondMetadataBytes) == originalAudioPayload,
            "Repeated RIFF edits must keep one INFO list and identical audio");

    std::istringstream clearMetadataInput(
        secondMetadataBytes, std::ios::binary);
    std::ostringstream clearMetadataOutput(std::ios::binary);
    const auto clearReport = rewriteWavInfoMetadata(
        clearMetadataInput, clearMetadataOutput, {});
    const auto clearedMetadataBytes = clearMetadataOutput.str();
    require(clearReport.infoListsRemoved == 1
                && !clearReport.infoListWritten
                && clearedMetadataBytes.find("LIST") == std::string::npos
                && audioPayload(clearedMetadataBytes) == originalAudioPayload,
            "Empty RIFF metadata must remove INFO without touching audio");

    auto malformedMetadataBytes = metadataBytes;
    malformedMetadataBytes[4] = static_cast<char>(0xff);
    malformedMetadataBytes[5] = static_cast<char>(0xff);
    malformedMetadataBytes[6] = static_cast<char>(0xff);
    malformedMetadataBytes[7] = static_cast<char>(0x7f);
    rejected = false;
    try
    {
        std::istringstream malformedInput(
            malformedMetadataBytes, std::ios::binary);
        std::ostringstream malformedOutput(std::ios::binary);
        static_cast<void>(rewriteWavInfoMetadata(
            malformedInput, malformedOutput, {"Unsafe", "", "", "", ""}));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected,
            "RIFF metadata rewrite must reject inconsistent declared sizes");

    const auto peaks = buildWaveformPeaks(*decodedWav, 1);
    require(peaks.size() == 1 && peaks[0].minimumLeft < -0.99F
                && peaks[0].maximumLeft > 0.24F
                && peaks[0].minimumRight < -0.24F
                && peaks[0].maximumRight > 0.99F,
            "Waveform cache must preserve stereo minimum and maximum peaks");
    rejected = false;
    try
    {
        static_cast<void>(buildWaveformPeaks(*decodedWav, maxWaveformBins + 1));
    }
    catch (const std::out_of_range&)
    {
        rejected = true;
    }
    require(rejected, "Waveform cache must enforce its resolution ceiling");

    AssistedRng assisted;
    const std::array<double, 5> javascriptGolden {
        0.30626874952577055,
        0.70605792384594679,
        0.45527462591417134,
        0.87445243168622255,
        0.68563826568424702
    };
    for (const auto expected : javascriptGolden)
        require(approximately(assisted.next(), expected),
                "Assisted RNG must match the JavaScript Mulberry32 sequence exactly");
    require(assisted.state() == 0x701ab7fbU && assisted.cursor() == 5,
            "Assisted RNG must preserve the reproducible state and cursor");
    assisted.rewind();
    require(approximately(assisted.next(), javascriptGolden[0]),
            "Rewinding an Assisted seed must reconstruct its first decision");
    std::uint32_t parsedSeed = 0;
    require(AssistedRng::parseSeed("0x00aBcD12", parsedSeed)
                && parsedSeed == 0x00abcd12U
                && AssistedRng::formatSeed(parsedSeed) == "00ABCD12",
            "Assisted seeds must parse and format as eight hexadecimal digits");
    require(!AssistedRng::parseSeed("not-a-seed", parsedSeed),
            "Invalid Assisted seed text must be rejected");

    const Json jsonDocument(Json::Object {
        {"format", "navalha-project"},
        {"version", 2},
        {"unicode", "Navalha \xE2\x9C\x82"},
        {"values", Json::Array {true, nullptr, -12.5}}
    });
    const auto jsonText = serializeJson(jsonDocument);
    const auto reparsedJson = parseJson(jsonText);
    require(reparsedJson.find("format") != nullptr
                && reparsedJson.find("format")->string() == "navalha-project"
                && reparsedJson.find("version")->number() == 2.0
                && reparsedJson.find("values")->array().size() == 3,
            "JSON codec must round-trip project objects, arrays and scalar values");
    rejected = false;
    try
    {
        static_cast<void>(parseJson("[[[[0]]]]", 1024, 2));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "JSON parser must enforce its nesting safety limit");

    require(isSafePortableRelativePath("audio/source-a.wav")
                && normalizePortableRelativePath("audio\\source-b.wav")
                    == "audio/source-b.wav",
            "Portable paths must accept and normalize safe relative entries");
    for (const auto unsafe : {
             "../source.wav", "audio/../../escape.wav", "/etc/passwd",
             "C:\\audio\\source.wav", "audio//source.wav", "./source.wav"})
        require(!isSafePortableRelativePath(unsafe),
                "Portable packs must reject traversal and non-relative paths");

    const std::vector<std::uint8_t> projectJsonBytes(projectJson.begin(), projectJson.end());
    const std::vector<std::uint8_t> sourceAudioBytes(encodedBytes.begin(), encodedBytes.end());
    const auto portableArchive = encodePortableArchive({
        {"project.navalha", projectJsonBytes},
        {"audio/source-a.wav", sourceAudioBytes}
    });
    const auto portableEntries = decodePortableArchive(portableArchive);
    require(portableEntries.size() == 2
                && portableEntries[0].path == "project.navalha"
                && portableEntries[0].data == projectJsonBytes
                && portableEntries[1].data == sourceAudioBytes,
            "Portable ZIP must round-trip project and source audio bytes");

    rejected = false;
    try
    {
        static_cast<void>(encodePortableArchive({{"../escape.wav", {1, 2, 3}}}));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "Portable ZIP creation must reject path traversal");

    auto corruptedArchive = portableArchive;
    const auto firstDataOffset = 30U + std::string("project.navalha").size();
    corruptedArchive[firstDataOffset] ^= 0x01U;
    rejected = false;
    try
    {
        static_cast<void>(decodePortableArchive(corruptedArchive));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "Portable ZIP extraction must reject CRC-corrupted entries");

    const auto legacyNvl = parseLegacyNvl(
        "# historical Navalha preset\n"
        "filename archive/source.wav;\n"
        "pattern archive/patterns.ptn;\n"
        "start 0 0;\nend 0 0.25;\n"
        "start 1 0.25;\nend 1 0.5;\n"
        "start 3 0;\nend 3 0;\n"
        "unknown preserved value;\n",
        "breakcore.nvl");
    require(legacyNvl.name == "breakcore.nvl"
                && legacyNvl.sampleReference == "archive/source.wav"
                && legacyNvl.patternReference == "archive/patterns.ptn"
                && legacyNvl.storedSlices.size() == 4
                && legacyNvl.operationalCount == 2
                && legacyNvl.incompleteIndices.size() == 1
                && legacyNvl.incompleteIndices[0] == 2
                && legacyNvl.unknownLines.size() == 1,
            "Legacy NVL parser must preserve references, padded slices and diagnostics");

    const auto legacyPatterns = parseLegacyPatterns(
        "#matrix 16 4\n6 37 10 20;\n4 6 5 6 0 5 0 1\n",
        "breakcore.ptn");
    require(legacyPatterns.name == "breakcore.ptn"
                && legacyPatterns.declaredRows == 16
                && legacyPatterns.declaredColumns == 4
                && legacyPatterns.sourceRows == 2
                && legacyPatterns.rows[0][0] == 6
                && legacyPatterns.rows[0][3] == 20
                && legacyPatterns.rows[0][4] == 0
                && legacyPatterns.rows[1][7] == 1
                && legacyPatterns.warnings.size() == 3,
            "Legacy PTN parser must clamp/pad rows and report matrix differences");
    SliceBank legacyExportSlices;
    legacyExportSlices.divideRegion(0.0, 1.0, 4);
    PatternBank legacyExportPatterns;
    legacyExportPatterns.setCell(0, 0, 6);
    legacyExportPatterns.setCell(0, 1, 37);
    const auto exportedNvl = encodeLegacyNvl(
        "source.wav", "patterns.ptn", legacyExportSlices);
    const auto exportedPatterns = encodeLegacyPatterns(legacyExportPatterns);
    const auto reparsedNvl = parseLegacyNvl(exportedNvl);
    const auto reparsedPatterns = parseLegacyPatterns(exportedPatterns);
    require(reparsedNvl.storedSlices.size() == 4
                && approximately(reparsedNvl.storedSlices[1].start, 0.25)
                && reparsedPatterns.rows[0][0] == 6
                && reparsedPatterns.rows[0][1] == 37,
            "Legacy export must round-trip with the compatible parser");

    const auto navalhaPortable = createPortableProject(
        savedProject,
        {reinterpret_cast<const std::uint8_t*>(encodedBytes.data()), encodedBytes.size()});
    const auto openedPortable = openPortableProject(navalhaPortable);
    require(openedPortable.project.patterns.cell(3, 4) == 205
                && openedPortable.project.hasAlbumProject
                && openedPortable.project.albumProject.title
                    == "Ruptures in water"
                && openedPortable.project.albumProject.tracks.size() == 1
                && openedPortable.project.albumProject.tracks[0].takeId
                    == "take-1"
                && openedPortable.sourceA != nullptr
                && openedPortable.sourceA->size() == 2
                && openedPortable.sourceB == nullptr,
            "Navalha portable pack must restore editorial state and assigned source audio");

    const auto unwantedFileArchive = encodePortableArchive({
        {"project.navalha", projectJsonBytes},
        {"scripts/unwanted.exe", {1, 2, 3}}
    });
    rejected = false;
    try
    {
        static_cast<void>(openPortableProject(unwantedFileArchive));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "Navalha portable packs must reject every unexpected file");

    const StereoAudioBuffer masteringFixture(
        48000.0,
        {0.5F, -0.5F, 0.5F, -0.5F},
        {0.5F, -0.5F, 0.5F, -0.5F});
    const auto masteringMetrics = analyzeForMastering(masteringFixture);
    require(masteringMetrics.inspectedFrames == 4
                && approximately(masteringMetrics.peak, 0.5)
                && approximately(masteringMetrics.rms, 0.5)
                && approximately(masteringMetrics.peakDb,
                                 20.0 * std::log10(0.5))
                && approximately(masteringMetrics.estimatedLufs,
                                 -0.691 + 10.0 * std::log10(0.25))
                && approximately(masteringMetrics.correlation, 1.0)
                && approximately(masteringMetrics.crestDb, 0.0),
            "MASTER analysis must match the v0.28.1 internal meter");
    require(approximately(
                recommendedLoudnessTrimDb(masteringMetrics, -14.0), -6.0),
            "Album loudness matching must retain the historical +/-6 dB limit");
    auto louderPreviewMetrics = masteringMetrics;
    louderPreviewMetrics.estimatedLufs = -8.0;
    auto quieterPreviewMetrics = masteringMetrics;
    quieterPreviewMetrics.estimatedLufs = -14.0;
    require(approximately(
                matchedPreviewAttenuationDb(
                    louderPreviewMetrics, quieterPreviewMetrics),
                -6.0)
                && approximately(
                    matchedPreviewAttenuationDb(
                        quieterPreviewMetrics, louderPreviewMetrics),
                    0.0),
            "MASTER A/B matching must attenuate only the louder side");

    const StereoAudioBuffer antiPhaseFixture(
        48000.0, {0.25F, -0.25F}, {-0.25F, 0.25F});
    require(approximately(
                analyzeForMastering(antiPhaseFixture).correlation, -1.0),
            "MASTER analysis must detect anti-phase stereo material");
    rejected = false;
    try
    {
        static_cast<void>(analyzeForMastering(masteringFixture, 0));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "MASTER analysis must reject an unbounded zero limit");

    const std::vector<AlbumTrackSettings> albumTracks {
        {1.0, 0.0, 2.0, 0.1, 0.2},
        {0.5, -6.0, 0.0, 1.0, 1.0}
    };
    const auto albumLayout = planAlbumLayout(albumTracks, 48000.0);
    require(albumLayout.tracks.size() == 2
                && albumLayout.tracks[0].startFrame == 0
                && albumLayout.tracks[0].audioFrames == 48000
                && albumLayout.tracks[0].gapFrames == 96000
                && albumLayout.tracks[1].startFrame == 144000
                && albumLayout.tracks[1].fadeInFrames == 12000
                && albumLayout.tracks[1].fadeOutFrames == 12000
                && albumLayout.totalFrames == 168000
                && std::abs(albumLayout.tracks[1].linearGain
                            - std::pow(10.0, -6.0 / 20.0)) < 1.0e-12,
            "ALBUM MASTER must plan gaps, fades and trims by exact frames");
    require(approximately(albumTrackEnvelopeGain(albumLayout.tracks[0], 0), 0.0)
                && approximately(
                    albumTrackEnvelopeGain(albumLayout.tracks[0], 2400), 0.5)
                && approximately(
                    albumTrackEnvelopeGain(albumLayout.tracks[0], 4800), 1.0)
                && approximately(
                    albumTrackEnvelopeGain(albumLayout.tracks[0], 47999),
                    1.0 / 9600.0)
                && approximately(
                    albumTrackEnvelopeGain(albumLayout.tracks[0], 48000), 0.0),
            "ALBUM MASTER fades must follow the WebAudio linear envelope");
    rejected = false;
    try
    {
        static_cast<void>(planAlbumLayout(
            std::vector<AlbumTrackSettings>(maximumAlbumTracks + 1), 48000.0));
    }
    catch (const std::length_error&)
    {
        rejected = true;
    }
    require(rejected, "ALBUM MASTER must enforce the 99-track limit");

    const StereoAudioBuffer masteringRenderFixture(
        48000.0,
        {0.0F, 0.2F, -0.3F, 0.7F, -0.8F, 0.1F},
        {0.0F, -0.1F, 0.4F, 0.5F, -0.6F, -0.2F});
    MasteringParameters masteringParameters;
    masteringParameters.trimDb = 1.5;
    masteringParameters.lowShelfDb = 1.0;
    masteringParameters.presenceDb = -0.5;
    masteringParameters.highShelfDb = 0.75;
    masteringParameters.saturation = 0.15;
    masteringParameters.width = 1.2;
    const auto masteredA = renderMastering(
        masteringRenderFixture, masteringParameters);
    const auto masteredB = renderMastering(
        masteringRenderFixture, masteringParameters);
    require(masteredA.left.size() == masteringRenderFixture.size()
                && masteredA.right.size() == masteringRenderFixture.size()
                && masteredA.left == masteredB.left
                && masteredA.right == masteredB.right
                && std::all_of(masteredA.left.begin(), masteredA.left.end(),
                               [](float value) { return std::isfinite(value); })
                && std::all_of(masteredA.right.begin(), masteredA.right.end(),
                               [](float value) { return std::isfinite(value); }),
            "TRACK MASTER render must be finite and deterministic");
    rejected = false;
    try
    {
        auto invalidMastering = masteringParameters;
        invalidMastering.width = 3.0;
        static_cast<void>(renderMastering(
            masteringRenderFixture, invalidMastering));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "TRACK MASTER must reject unsafe parameter ranges");
    rejected = false;
    try
    {
        static_cast<void>(renderMastering(
            masteringRenderFixture, masteringParameters, 5));
    }
    catch (const std::length_error&)
    {
        rejected = true;
    }
    require(rejected, "TRACK MASTER must enforce its offline frame limit");

    TakeCatalog takeCatalog;
    TakeEntry takeEntry;
    takeEntry.id = "take-20260729-001";
    takeEntry.audioPath = "/tmp/NAVALHA_2026-07-29_18-00-00.wav";
    takeEntry.filename = "NAVALHA_2026-07-29_18-00-00.wav";
    takeEntry.createdAt = "2026-07-29T18:00:00Z";
    takeEntry.durationSeconds = 12.5;
    takeEntry.frames = 600000;
    takeEntry.sampleRate = 48000;
    takeEntry.sampleFormat = WavSampleFormat::pcm24;
    takeEntry.metadata = {
        "Take title", "Lúcio Araújo", "Navalha 2", "2026", "First take"};
    takeEntry.review = {"SELECTED", 4, "continuous, rupture", "Keep ending"};
    takeEntry.recipeJson =
        R"({"format":"navalha-take-recipe","version":1,"seed":"0x12345678"})";
    takeCatalog.upsert(takeEntry);
    const auto takeCatalogJson = encodeTakeCatalog(takeCatalog);
    const auto decodedTakeCatalog = decodeTakeCatalog(takeCatalogJson);
    const auto* decodedTake = decodedTakeCatalog.find(takeEntry.id);
    require(decodedTake != nullptr
                && decodedTake->audioPath == takeEntry.audioPath
                && decodedTakeCatalog.findByAudioPath(takeEntry.audioPath)
                    == decodedTake
                && decodedTake->sampleFormat == WavSampleFormat::pcm24
                && decodedTake->review.status == "SELECTED"
                && decodedTake->review.rating == 4
                && decodedTake->metadata.artist == "Lúcio Araújo"
                && decodedTake->recipeJson.find("navalha-take-recipe")
                    != std::string::npos,
            "TAKE catalog v1 must preserve provenance, review and recipe");
    WavMetadata oversizedPreset {
        std::string(200, 'T'), std::string(200, 'A'),
        std::string(200, 'P'), std::string(20, 'Y'),
        std::string(600, 'C')};
    normalizeWavMetadata(oversizedPreset);
    require(oversizedPreset.title.size() == 160
                && oversizedPreset.artist.size() == 160
                && oversizedPreset.project.size() == 160
                && oversizedPreset.year.size() == 12
                && oversizedPreset.comment.size() == 500,
            "Recording metadata presets must share TAKE catalog text limits");
    takeEntry.review.status = "UNKNOWN";
    takeEntry.review.rating = 99;
    takeCatalog.upsert(takeEntry);
    require(takeCatalog.find(takeEntry.id)->review.status == "EXPERIMENT"
                && takeCatalog.find(takeEntry.id)->review.rating == 5
                && takeCatalog.entries().size() == 1,
            "TAKE catalog must normalize review values and upsert by id");
    rejected = false;
    try
    {
        static_cast<void>(decodeTakeCatalog(
            R"({"format":"navalha-take-catalog","version":2,"takes":[]})"));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "TAKE catalog must reject unsupported versions");

    const MasteringRecipe masteringRecipe {
        "mix.wav", "2026-07-29T18:00:00Z", masteringParameters
    };
    const auto masteringRecipeJson = encodeMasteringRecipe(masteringRecipe);
    const auto decodedMasteringRecipe =
        decodeMasteringRecipe(masteringRecipeJson);
    require(decodedMasteringRecipe.source == masteringRecipe.source
                && decodedMasteringRecipe.createdAt == masteringRecipe.createdAt
                && approximately(decodedMasteringRecipe.parameters.trimDb,
                                 masteringParameters.trimDb)
                && approximately(decodedMasteringRecipe.parameters.highPassHz,
                                 masteringParameters.highPassHz)
                && approximately(decodedMasteringRecipe.parameters.width,
                                 masteringParameters.width)
                && approximately(decodedMasteringRecipe.parameters.saturation,
                                 masteringParameters.saturation),
            "MASTER recipe v1 must preserve WebAudio-compatible parameters");
    rejected = false;
    try
    {
        static_cast<void>(decodeMasteringRecipe(
            R"({"format":"navalha-master-recipe","version":2,"parameters":{}})"));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "MASTER recipe must reject unsupported versions");

    AlbumMasterManifest albumManifest;
    albumManifest.createdAt = "2026-07-29T18:30:00Z";
    albumManifest.title = "Validation Album";
    albumManifest.artist = "Navalha 2";
    albumManifest.chain = masteringParameters;
    AlbumManifestTrack manifestTrack;
    manifestTrack.id = "track-1";
    manifestTrack.title = "Diagnostic";
    manifestTrack.filename = "audio/diagnostic.wav";
    manifestTrack.settings = {4.0, 0.75, 0.5, 0.1, 0.2};
    manifestTrack.hasAnalysis = true;
    manifestTrack.analysis = masteringMetrics;
    albumManifest.tracks.push_back(manifestTrack);
    const auto albumManifestJson = encodeAlbumMasterManifest(albumManifest);
    const auto decodedAlbumManifest =
        decodeAlbumMasterManifest(albumManifestJson);
    require(decodedAlbumManifest.title == albumManifest.title
                && decodedAlbumManifest.artist == albumManifest.artist
                && decodedAlbumManifest.tracks.size() == 1
                && decodedAlbumManifest.tracks[0].filename
                    == manifestTrack.filename
                && decodedAlbumManifest.tracks[0].hasAnalysis
                && approximately(
                    decodedAlbumManifest.tracks[0].settings.gapAfterSeconds,
                    0.5)
                && approximately(decodedAlbumManifest.chain.width,
                                 masteringParameters.width),
            "ALBUM MASTER manifest v1 must round-trip chain and track data");
    rejected = false;
    try
    {
        auto unsafeAlbum = albumManifest;
        unsafeAlbum.tracks[0].filename = "../escape.wav";
        static_cast<void>(encodeAlbumMasterManifest(unsafeAlbum));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected,
            "ALBUM MASTER manifest must reject unsafe audio references");

    AlbumProject albumProject;
    auto projectTakeA = takeEntry;
    projectTakeA.id = "take-album-a";
    projectTakeA.filename = "album-a.wav";
    projectTakeA.metadata.title = "Opening";
    projectTakeA.metadata.artist = "Navalha 2";
    projectTakeA.metadata.project = "Validation Album";
    projectTakeA.durationSeconds = 4.0;
    auto projectTakeB = projectTakeA;
    projectTakeB.id = "take-album-b";
    projectTakeB.filename = "album-b.wav";
    projectTakeB.metadata.title = "Closing";
    projectTakeB.durationSeconds = 7.5;
    require(addTakeToAlbumProject(albumProject, projectTakeA)
                && addTakeToAlbumProject(albumProject, projectTakeB)
                && !addTakeToAlbumProject(albumProject, projectTakeA)
                && albumProject.title == "Validation Album"
                && albumProject.artist == "Navalha 2"
                && albumProject.tracks.size() == 2,
            "ALBUM PROJECT must add each TAKE once and infer album metadata");
    require(moveAlbumProjectTrack(albumProject, 1, -1)
                && albumProject.tracks.front().takeId == projectTakeB.id
                && !moveAlbumProjectTrack(albumProject, 0, -1),
            "ALBUM PROJECT must preserve explicit bounded track order");
    auto quietAlbumMetrics = masteringMetrics;
    quietAlbumMetrics.estimatedLufs = -22.0;
    const std::array albumProjectAnalysis {
        quietAlbumMetrics, masteringMetrics};
    matchAlbumProjectRelativeLevels(
        albumProject, albumProjectAnalysis, -14.0);
    require(albumProject.tracks[0].hasAnalysis
                && approximately(
                    albumProject.tracks[0].analysis.estimatedLufs, -22.0)
                && approximately(albumProject.tracks[0].settings.trimDb, 6.0)
                && approximately(albumProject.tracks[1].settings.trimDb, -6.0),
            "ALBUM PROJECT relative matching must persist analysis and +/-6 dB trims");
    const auto albumProjectJson = encodeAlbumProject(
        albumProject, "2026-08-09T20:00:00Z");
    const auto decodedAlbumProject = decodeAlbumProject(albumProjectJson);
    require(decodedAlbumProject.title == albumProject.title
                && decodedAlbumProject.tracks.size() == 2,
            "ALBUM PROJECT v1 must preserve album metadata and track count");
    require(decodedAlbumProject.tracks.front().takeId == projectTakeB.id,
            "ALBUM PROJECT v1 must preserve explicit track order");
    require(decodedAlbumProject.tracks.front().hasAnalysis
                && approximately(
                    decodedAlbumProject.tracks.front().analysis.estimatedLufs,
                    -22.0)
                && approximately(
                    decodedAlbumProject.tracks.front().settings.trimDb, 6.0),
            "ALBUM PROJECT v1 must preserve relative matching evidence");
    require(decodedAlbumProject.tracks.front().recipeJson.find(
                "navalha-take-recipe") != std::string::npos
                && decodedAlbumProject.tracks.front().recipeJson.find(
                    "0x12345678") != std::string::npos,
            "ALBUM PROJECT v1 must preserve the TAKE receipt");
    require(decodedAlbumProject.tracks.front().review.rating == 5
                && decodedAlbumProject.tracks.front().review.status
                    == "EXPERIMENT",
            "ALBUM PROJECT v1 must preserve the normalized TAKE review");
    rejected = false;
    try
    {
        const std::array<MasteringMetrics, 1> incompleteAnalysis {
            masteringMetrics};
        matchAlbumProjectRelativeLevels(
            albumProject, incompleteAnalysis, -14.0);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected,
            "ALBUM PROJECT matching must require one analysis per track");
    require(removeAlbumProjectTrack(albumProject, 0)
                && albumProject.tracks.size() == 1
                && !removeAlbumProjectTrack(albumProject, 5),
            "ALBUM PROJECT must remove only an existing ordered track");
    rejected = false;
    try
    {
        static_cast<void>(decodeAlbumProject(
            R"({"format":"navalha-album-project","version":2,"tracks":[]})"));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "ALBUM PROJECT must reject unsupported versions");

    std::cout << "Navalha JUCE migration core contracts: OK\n";
    return 0;
}
