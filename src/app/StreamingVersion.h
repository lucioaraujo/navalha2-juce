// Copyright (C) 2026 Lúcio de Araújo — GPL-3.0-or-later (o núcleo copiado da saida-ref é AGPL-3.0-or-later; GPLv3 §13).
// Navalha 2 — versão de entrega (streaming) da tomada do REC.
//
// Master de entrega (16/11/2026, saida-ref 1.1; o autor: "nivele todos os
// instrumentos para que fiquem preparados para os spotifys da vida"; opção 1,
// "como os outros", 17/11). Ao lado da tomada, <tomada>_streaming.wav a
// -14 LUFS / <= -1 dBTP e um .txt. Lida em blocos pelo leitor do JUCE,
// escrita pelo WavStreamWriter do Navalha (PCM 24 com a TPDF dele). O painel
// MASTER continua para masterizar à mão; o som ao vivo e a tomada não mudam.
#pragma once

#include "core/WavStreamWriter.h"
#include "core/saida/DeliveryMaster.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace navalha
{
inline void writeStreamingVersion(const juce::File& take)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(take));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return;
    struct ReaderSource final : saida::DeliveryMaster::Source
    {
        juce::AudioFormatReader& in;
        juce::int64 position = 0;
        juce::AudioBuffer<float> buffer {2, 8192};
        explicit ReaderSource(juce::AudioFormatReader& r) : in(r) {}
        bool rewind() override { position = 0; return true; }
        std::size_t read(float* left, float* right, std::size_t frames) override
        {
            const auto count = static_cast<int>(std::min<juce::int64>(
                static_cast<juce::int64>(std::min<std::size_t>(frames, 8192)), in.lengthInSamples - position));
            if (count <= 0 || !in.read(&buffer, 0, count, position, true, true))
                return 0;
            std::copy_n(buffer.getReadPointer(0), count, left);
            std::copy_n(buffer.getReadPointer(in.numChannels > 1 ? 1 : 0), count, right);
            position += count;
            return static_cast<std::size_t>(count);
        }
    } source(*reader);
    const auto targetPath = std::filesystem::path(
        take.getSiblingFile(take.getFileNameWithoutExtension() + "_streaming.wav").getFullPathName().toStdString());
    auto partial = targetPath;
    partial += ".partial";
    std::ofstream output(partial, std::ios::binary);
    if (!output)
        return;
    WavMetadata metadata;
    metadata.title = "Navalha 2 (streaming)";
    metadata.artist = "Lucio de Araujo";
    metadata.comment = "Versao de entrega: -14 LUFS, true peak <= -1 dBTP (saida-ref 1.2)";
    struct WriterSink final : saida::DeliveryMaster::Sink
    {
        WavStreamWriter& out;
        explicit WriterSink(WavStreamWriter& w) : out(w) {}
        bool write(const float* left, const float* right, std::size_t frames) override
        {
            for (std::size_t i = 0; i < frames; ++i)
                out.writeFrame({left[i], right[i]});
            return true;
        }
    };
    saida::DeliveryMaster::Report report;
    try
    {
        WavStreamWriter writer(output, static_cast<std::uint32_t>(reader->sampleRate), WavSampleFormat::pcm24, metadata);
        WriterSink sink(writer);
        report = saida::DeliveryMaster::process(source, reader->sampleRate, sink);
        writer.finalize();
    }
    catch (...)
    {
        report.failed = true;
    }
    output.close();
    std::error_code error;
    if (report.failed || !output)
    {
        std::filesystem::remove(partial, error);
        return;
    }
    std::filesystem::rename(partial, targetPath, error);
    if (error)
        return;
    std::ostringstream text;
    text << "Navalha 2 - versao de entrega (streaming), saida-ref 1.2\n"
         << "tomada: " << take.getFileName().toStdString() << "\n"
         << "alvo: -14 LUFS, true peak <= -1 dBTP; limitador no maximo 6 dB (guarda de dinamica)\n"
         << "entrada: " << report.inputLufs << " LUFS, " << report.inputTruePeakDb << " dBTP\n"
         << "ganho: " << report.gainDb << " dB; limitador: " << report.limiterReductionDb << " dB\n"
         << "saida: " << report.outputLufs << " LUFS, " << report.outputTruePeakDb << " dBTP\n"
         << (report.silent ? "silencio: copiada como veio\n"
             : report.reachedTarget ? "alvo atingido\n"
             : "alvo nao atingido sem esmagar a dinamica: entregue no mais alto possivel (os servicos sobem faixas baixas no modo normal)\n");
    std::ofstream(std::filesystem::path(targetPath).replace_extension(".txt")) << text.str();
}
}
