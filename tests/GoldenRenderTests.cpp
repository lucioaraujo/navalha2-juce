#include "core/AudioEngine.h"
#include "core/MasteringProcessor.h"
#include "core/OfflineRenderer.h"
#include "core/SessionModel.h"
#include "core/SlicePlayer.h"
#include "core/WavMemoryReader.h"
#include "core/WavStreamWriter.h"

#include <cmath>
#include <bit>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
std::uint64_t hashBytes(const std::string& bytes)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto byte : bytes)
    {
        hash ^= static_cast<std::uint8_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::uint64_t hashMastering(const navalha::MasteringRender& render)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t frame = 0; frame < render.left.size(); ++frame)
    {
        hash ^= std::bit_cast<std::uint32_t>(render.left[frame]);
        hash *= 1099511628211ULL;
        hash ^= std::bit_cast<std::uint32_t>(render.right[frame]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

navalha::OfflineRender renderFixture(std::size_t blockSize)
{
    constexpr double sampleRate = 48000.0;
    constexpr std::size_t frameCount = 48000;
    std::vector<float> left(frameCount);
    std::vector<float> right(frameCount);
    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        const auto time = static_cast<double>(frame) / sampleRate;
        left[frame] = static_cast<float>(
            0.42 * std::sin(2.0 * std::numbers::pi * 220.0 * time)
            + 0.13 * std::sin(2.0 * std::numbers::pi * 997.0 * time));
        right[frame] = static_cast<float>(
            0.31 * std::sin(2.0 * std::numbers::pi * 330.0 * time));
    }

    navalha::StereoAudioBuffer source(sampleRate, std::move(left), std::move(right));
    navalha::SessionModel session;
    session.patterns.setCell(0, 0, 0);
    session.mixer.sourceA.level = 0.91;
    session.mixer.sourceA.pan = -0.17;
    session.mixer.sourceA.width = 1.23;
    session.masterLevel = 0.84;
    session.heritagePitchSemitones = -5;
    session.heritagePitchMode = 0.67;

    navalha::AudioEngine engine(session);
    engine.prepare(sampleRate);
    engine.setSourceBuffer(0, &source);
    require(engine.submitCommand({navalha::EngineCommandType::start}),
            "Golden transport command failed");
    return navalha::renderOffline(engine, frameCount, blockSize);
}
}

int main()
{
    constexpr std::uint64_t expectedRenderChecksum = 3440523282391997346ULL;
    constexpr std::uint64_t expectedWavChecksum = 5403870701564285649ULL;
    // Regenerated 18 ago. 2026 after fixing AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md
    // 3.4 (real LookaheadLimiter instead of a no-lookahead ratio-20
    // compressor) and 3.5 (saturation=0 is now a true bypass) in
    // MasteringProcessor - the golden signature intentionally changed, this
    // is not a regression. Value measured on this session's toolchain (GCC
    // 13.3.0, Debug, local build) via navalha_golden_render_tests itself.
    constexpr std::uint64_t expectedMasteringChecksum = 1536196086264323730ULL;
    // The mastering chain is intentionally float based. GCC 11 Release on
    // Ubuntu 22.04 contracted one intermediate differently from the local
    // Debug toolchain under the OLD DSP (pre-18 ago. 2026 fix) - this second
    // signature has NOT been regenerated for the new DSP above (no access to
    // that exact toolchain/config from this session). Treat a mismatch
    // against this specific constant on that platform as expected until it
    // is re-measured there, not as a silent regression.
    constexpr std::uint64_t expectedUbuntu2204MasteringChecksum =
        12625143991486910343ULL;
    const auto render = renderFixture(127);
    const auto alternate = renderFixture(1024);
    require(render.checksum == alternate.checksum,
            "Golden render changed with host block size");
    require(render.peak == alternate.peak
                && render.meanSquare == alternate.meanSquare,
            "Golden render measurements changed with host block size");

    std::ostringstream output(std::ios::binary);
    navalha::WavStreamWriter writer(
        output, 48000, navalha::WavSampleFormat::pcm24,
        {"Navalha 2 golden render", "Navalha 2", "JUCE migration", "2026",
         "Deterministic regression fixture"},
        {navalha::WavDitherMode::none});
    for (std::size_t frame = 0; frame < render.left.size(); ++frame)
        writer.writeFrame({render.left[frame], render.right[frame]});
    writer.finalize();

    const auto wav = output.str();
    require(render.checksum == expectedRenderChecksum,
            "DSP golden signature changed: " + std::to_string(render.checksum));
    require(hashBytes(wav) == expectedWavChecksum,
            "PCM24 golden WAV signature changed: "
                + std::to_string(hashBytes(wav)));
    const auto decoded = navalha::decodeWav({
        reinterpret_cast<const std::uint8_t*>(wav.data()), wav.size()});
    require(decoded->size() == render.left.size()
                && decoded->sampleRate() == 48000.0,
            "Golden WAV did not round-trip");

    navalha::MasteringParameters masteringParameters;
    masteringParameters.trimDb = 1.2;
    masteringParameters.lowShelfDb = 0.8;
    masteringParameters.presenceDb = -0.4;
    masteringParameters.highShelfDb = 1.1;
    masteringParameters.compressorThresholdDb = -14.0;
    masteringParameters.compressorRatio = 3.0;
    masteringParameters.width = 1.18;
    masteringParameters.saturation = 0.12;
    const navalha::StereoAudioBuffer masteringSource(
        48000.0, render.left, render.right);
    const auto mastered = navalha::renderMastering(
        masteringSource, masteringParameters);
    const auto masteringChecksum = hashMastering(mastered);
    require(masteringChecksum == expectedMasteringChecksum
                || masteringChecksum == expectedUbuntu2204MasteringChecksum,
            "TRACK MASTER golden signature changed: "
                + std::to_string(masteringChecksum));

    std::cout << "render_checksum=" << expectedRenderChecksum
              << " wav_checksum=" << expectedWavChecksum
              << " mastering_checksum=" << expectedMasteringChecksum
              << " peak=" << render.peak
              << " mean_square=" << render.meanSquare << '\n';
}
