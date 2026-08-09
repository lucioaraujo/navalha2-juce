#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "BinaryData.h"
#include "UiHelp.h"

#if JUCE_LINUX
#include <X11/Xlib.h>
#endif

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/AudioEngine.h"
#include "core/Json.h"
#include "core/MasteringAlbumManifest.h"
#include "core/MasteringAnalysis.h"
#include "core/MasteringProcessor.h"
#include "core/MasteringRecipe.h"
#include "core/PortablePath.h"
#include "core/PortableProject.h"
#include "core/ProjectJson.h"
#include "core/ProjectState.h"
#include "core/RecordingWriterService.h"
#include "core/SessionModel.h"
#include "core/TakeCatalog.h"
#include "core/WavMemoryReader.h"
#include "core/WavStreamWriter.h"
#include "core/WaveformPeaks.h"

namespace
{
namespace Arcade
{
constexpr auto background = 0xff080a0a;
constexpr auto surface = 0xff101313;
constexpr auto surfaceHigh = 0xff171b1b;
constexpr auto line = 0xff343b3b;
constexpr auto ink = 0xfff4f5f1;
constexpr auto muted = 0xffa2a8a6;
constexpr auto yellow = 0xffffd84a;
constexpr auto yellowHigh = 0xfffff08a;
constexpr auto steel = 0xff78828a;
constexpr auto red = 0xffe01820;
constexpr int contextualRailWidth = 340;
}

juce::Colour tintedPanelSurface(juce::Colour accent, float amount = 0.16F)
{
    return juce::Colour(Arcade::surface).interpolatedWith(
        accent, amount);
}

void styleEditableTextField(juce::TextEditor& editor)
{
    editor.setColour(
        juce::TextEditor::backgroundColourId,
        tintedPanelSurface(juce::Colour(Arcade::yellow), 0.13F));
    editor.setColour(
        juce::TextEditor::outlineColourId,
        juce::Colour(Arcade::line).interpolatedWith(
            juce::Colour(Arcade::yellow), 0.52F));
    editor.setColour(
        juce::TextEditor::focusedOutlineColourId,
        juce::Colour(Arcade::yellowHigh));
    editor.setColour(
        juce::TextEditor::textColourId, juce::Colour(Arcade::ink));
    editor.setColour(
        juce::TextEditor::highlightColourId,
        juce::Colour(Arcade::yellow).withAlpha(0.42F));
    editor.setColour(
        juce::TextEditor::highlightedTextColourId,
        juce::Colour(Arcade::ink));
    editor.setColour(
        juce::CaretComponent::caretColourId, juce::Colour(Arcade::yellowHigh));
    editor.setFont(juce::Font(juce::FontOptions(
        "DejaVu Sans Mono", 11.0F, juce::Font::plain)));
    editor.setIndents(7, 4);
}

juce::String utf8(std::string_view value)
{
    return juce::String::fromUTF8(
        value.data(), static_cast<int>(value.size()));
}

juce::Image navalhaAppIcon()
{
    static const auto icon = juce::ImageCache::getFromMemory(
        BinaryData::navalha2appicon128_png,
        BinaryData::navalha2appicon128_pngSize);
    return icon;
}

struct DecodedSourceFile
{
    std::unique_ptr<navalha::StereoAudioBuffer> audio;
    std::vector<std::uint8_t> portableWav;
    std::string mediaType = "audio/wav";
};

DecodedSourceFile decodeSourceFile(const juce::File& file)
{
    constexpr juce::int64 maximumInputBytes = 512LL * 1024LL * 1024LL;
    if (!file.existsAsFile() || file.getSize() <= 0
        || file.getSize() > maximumInputBytes)
        throw std::length_error("Audio file exceeds the 512 MiB safety limit");

    juce::MemoryBlock bytes;
    if (!file.loadFileAsData(bytes))
        throw std::runtime_error("Unable to read audio file");

    DecodedSourceFile result;
    if (file.hasFileExtension("wav;wave"))
    {
        result.audio = navalha::decodeWav({
            static_cast<const std::uint8_t*>(bytes.getData()),
            bytes.getSize()});
        result.portableWav.assign(
            static_cast<const std::uint8_t*>(bytes.getData()),
            static_cast<const std::uint8_t*>(bytes.getData()) + bytes.getSize());
        return result;
    }

    if (!file.hasFileExtension("aif;aiff"))
        throw std::invalid_argument("Only WAV and AIFF sources are supported");

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(
        formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0
        || reader->lengthInSamples
            > static_cast<juce::int64>(navalha::maxDecodedAudioFrames))
        throw std::invalid_argument("Unsupported or oversized AIFF file");

    const auto frames = static_cast<std::size_t>(reader->lengthInSamples);
    juce::AudioBuffer<float> decoded(
        2, static_cast<int>(reader->lengthInSamples));
    if (!reader->read(
            &decoded, 0, static_cast<int>(reader->lengthInSamples), 0, true, true))
        throw std::runtime_error("Unable to decode AIFF file");

    std::vector<float> left(frames);
    std::vector<float> right(frames);
    std::copy_n(decoded.getReadPointer(0), frames, left.begin());
    const auto rightChannel = reader->numChannels > 1 ? 1 : 0;
    std::copy_n(decoded.getReadPointer(rightChannel), frames, right.begin());
    result.audio = std::make_unique<navalha::StereoAudioBuffer>(
        reader->sampleRate, std::move(left), std::move(right));
    result.mediaType = "audio/aiff";

    std::ostringstream portable(std::ios::binary);
    navalha::WavStreamWriter writer(
        portable,
        static_cast<std::uint32_t>(std::lround(reader->sampleRate)),
        navalha::WavSampleFormat::pcm24,
        {file.getFileNameWithoutExtension().toStdString(), "", "Navalha 2",
         "", "AIFF source converted non-destructively for portable project"});
    for (std::size_t frame = 0; frame < result.audio->size(); ++frame)
        writer.writeFrame(result.audio->interpolated(static_cast<double>(frame)));
    writer.finalize();
    const auto encoded = portable.str();
    result.portableWav.assign(encoded.begin(), encoded.end());
    return result;
}

void publishMasterWav(const juce::File& outputFile,
                      double sampleRate,
                      const navalha::MasteringRender& rendered,
                      navalha::WavMetadata metadata)
{
    const std::filesystem::path finalPath(
        outputFile.getFullPathName().toStdString());
    const auto partialPath =
        std::filesystem::path(finalPath.string() + ".partial");
    if (std::filesystem::exists(finalPath)
        || std::filesystem::exists(partialPath))
        throw std::runtime_error("Output or partial output already exists");

    bool partialCreated = false;
    try
    {
        std::ofstream output(
            partialPath, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("Unable to create partial MASTER WAV");
        partialCreated = true;
        navalha::WavStreamWriter writer(
            output,
            static_cast<std::uint32_t>(std::lround(sampleRate)),
            navalha::WavSampleFormat::pcm24,
            std::move(metadata));
        for (std::size_t frame = 0; frame < rendered.left.size(); ++frame)
            writer.writeFrame({rendered.left[frame], rendered.right[frame]});
        writer.finalize();
        output.close();
        if (!output)
            throw std::runtime_error("Unable to finalize MASTER WAV");
        std::filesystem::rename(partialPath, finalPath);
        partialCreated = false;
    }
    catch (...)
    {
        if (partialCreated)
        {
            std::error_code ignored;
            std::filesystem::remove(partialPath, ignored);
        }
        throw;
    }
}

juce::String safeMasterStem(juce::String title)
{
    title = title.retainCharacters(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");
    return title.isEmpty() ? "track" : title.substring(0, 80);
}

class ArcadeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ArcadeLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId,
                  juce::Colour(Arcade::background));
        setColour(juce::Label::textColourId, juce::Colour(Arcade::ink));
        setColour(juce::TextButton::buttonColourId, juce::Colour(Arcade::surfaceHigh));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(Arcade::yellow));
        setColour(juce::TextButton::textColourOffId, juce::Colour(Arcade::ink));
        setColour(juce::TextButton::textColourOnId, juce::Colour(Arcade::background));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(Arcade::surface));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(Arcade::line));
        setColour(juce::ComboBox::textColourId, juce::Colour(Arcade::ink));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(Arcade::yellow));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(Arcade::surface));
        setColour(juce::PopupMenu::textColourId, juce::Colour(Arcade::ink));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,
                  juce::Colour(Arcade::yellow));
        setColour(juce::PopupMenu::highlightedTextColourId,
                  juce::Colour(Arcade::background));
        setColour(juce::Slider::trackColourId, juce::Colour(Arcade::steel));
        setColour(juce::Slider::thumbColourId, juce::Colour(Arcade::yellow));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(Arcade::ink));
        setColour(juce::Slider::textBoxBackgroundColourId,
                  juce::Colour(Arcade::background));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(Arcade::line));
        setColour(juce::ScrollBar::thumbColourId, juce::Colour(Arcade::steel));
        setColour(juce::ScrollBar::trackColourId, juce::Colour(Arcade::background));
        setColour(juce::ProgressBar::backgroundColourId, juce::Colour(Arcade::surface));
        setColour(juce::ProgressBar::foregroundColourId, juce::Colour(Arcade::yellow));
        setDefaultSansSerifTypefaceName("DejaVu Sans Mono");
    }

    juce::Font getTextButtonFont(juce::TextButton& button, int height) override
    {
        if (button.getProperties().contains("arcadeLargeButton"))
            return juce::Font(juce::FontOptions(
                "DejaVu Sans Mono", 14.0F, juce::Font::bold));
        return juce::Font(
            juce::FontOptions("DejaVu Sans Mono", juce::jlimit(11.0F, 15.0F,
                static_cast<float>(height) * 0.35F), juce::Font::bold));
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        if (label.getProperties().contains("arcadeClock"))
            return juce::Font(juce::FontOptions(
                "DejaVu Sans Mono", 18.0F, juce::Font::bold));
        if (label.getProperties().contains("arcadeFontSize"))
        {
            const auto size = static_cast<float>(
                static_cast<double>(label.getProperties()["arcadeFontSize"]));
            const auto style = static_cast<bool>(
                label.getProperties()["arcadeFontBold"])
                ? juce::Font::bold : juce::Font::plain;
            return juce::Font(juce::FontOptions(
                "DejaVu Sans Mono", juce::jlimit(9.0F, 26.0F, size), style));
        }
        return juce::Font(juce::FontOptions(
            "DejaVu Sans Mono",
            label.getProperties().contains("arcadeTitle") ? 23.0F : 12.0F,
            label.getProperties().contains("arcadeTitle")
                ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 11.0F, juce::Font::plain));
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 11.0F, juce::Font::plain));
    }

    juce::Slider::SliderLayout getSliderLayout(
        juce::Slider& slider) override
    {
        auto layout = juce::LookAndFeel_V4::getSliderLayout(slider);
        const auto textPosition = slider.getTextBoxPosition();
        if (!slider.isHorizontal() || slider.isBar()
            || (textPosition != juce::Slider::TextBoxLeft
                && textPosition != juce::Slider::TextBoxRight))
            return layout;

        const auto bounds = slider.getLocalBounds();
        const auto thumbIndent = getSliderThumbRadius(slider);
        // Always reserve at least as much visible width for the track as for
        // the numeric box, plus a small breathing gap. This automatically
        // contracts value boxes in dense dual-monitor rows.
        const auto maximumTextWidth = juce::jmax(
            16, (bounds.getWidth() - 2 * thumbIndent - 8) / 2);
        const auto textWidth = juce::jmin(
            layout.textBoxBounds.getWidth(), maximumTextWidth);
        layout.textBoxBounds.setWidth(textWidth);
        layout.textBoxBounds.setX(
            textPosition == juce::Slider::TextBoxLeft
                ? 0 : bounds.getWidth() - textWidth);

        layout.sliderBounds = bounds;
        if (textPosition == juce::Slider::TextBoxLeft)
            layout.sliderBounds.removeFromLeft(textWidth);
        else
            layout.sliderBounds.removeFromRight(textWidth);
        layout.sliderBounds.reduce(thumbIndent, 0);
        return layout;
    }

    void drawButtonBackground(juce::Graphics& graphics,
                              juce::Button& button,
                              const juce::Colour&,
                              bool highlighted,
                              bool down) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5F);
        const auto active = button.getToggleState() || down;
        const auto accentName =
            button.getProperties()["arcadeAccent"].toString();
        const auto accent = accentName == "record"
            ? juce::Colour(Arcade::red)
            : accentName == "sourceB"
                ? juce::Colour(Arcade::red)
                : accentName == "stop"
                    ? juce::Colour(Arcade::ink)
                    : juce::Colour(Arcade::yellow);
        auto top = active ? accent.brighter(0.28F)
                          : juce::Colour(highlighted ? 0xff222724 : 0xff1a1f1f);
        auto bottom = active ? accent
                             : juce::Colour(highlighted ? 0xff141817 : Arcade::surface);
        juce::ColourGradient gradient(top, bounds.getTopLeft(), bottom,
                                      bounds.getBottomLeft(), false);
        graphics.setGradientFill(gradient);
        graphics.fillRoundedRectangle(bounds, 3.0F);
        const auto hasAccent = accentName.isNotEmpty();
        graphics.setColour(active || hasAccent
            ? accent
            : juce::Colour(highlighted ? Arcade::yellow : Arcade::line));
        graphics.drawRoundedRectangle(bounds, 3.0F,
                                      hasAccent ? 1.5F : 1.0F);
    }

    void drawButtonText(juce::Graphics& graphics,
                        juce::TextButton& button,
                        bool,
                        bool down) override
    {
        graphics.setFont(getTextButtonFont(button, button.getHeight()));
        const auto active = button.getToggleState() || down;
        const auto accentName =
            button.getProperties()["arcadeAccent"].toString();
        const auto accent = accentName == "record"
            ? juce::Colour(Arcade::red)
            : accentName == "sourceB"
                ? juce::Colour(Arcade::red)
                : accentName == "stop"
                    ? juce::Colour(Arcade::ink)
                    : juce::Colour(Arcade::yellow);
        graphics.setColour(active
            ? juce::Colour(Arcade::background)
            : accentName.isNotEmpty() ? accent : juce::Colour(Arcade::ink));
        graphics.drawFittedText(button.getButtonText(),
                                button.getLocalBounds().reduced(5, 2),
                                juce::Justification::centred, 1);
    }

    void drawToggleButton(juce::Graphics& graphics,
                          juce::ToggleButton& button,
                          bool highlighted,
                          bool down) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5F);
        const auto active = button.getToggleState() || down;
        const auto accentName =
            button.getProperties()["arcadeAccent"].toString();
        const auto accent = accentName == "sourceB"
            ? juce::Colour(Arcade::red)
            : juce::Colour(Arcade::yellow);
        const auto top = active
            ? accent.brighter(0.28F)
            : juce::Colour(highlighted ? 0xff222724 : 0xff1a1f1f);
        const auto bottom = active
            ? accent
            : juce::Colour(highlighted ? 0xff141817 : Arcade::surface);
        juce::ColourGradient gradient(
            top, bounds.getTopLeft(), bottom, bounds.getBottomLeft(), false);
        graphics.setGradientFill(gradient);
        graphics.fillRoundedRectangle(bounds, 3.0F);
        graphics.setColour(active
            ? accent
            : highlighted ? accent : juce::Colour(Arcade::line));
        graphics.drawRoundedRectangle(bounds, 3.0F, active ? 1.5F : 1.0F);

        graphics.setColour(active
            ? juce::Colour(Arcade::background)
            : juce::Colour(Arcade::ink));
        graphics.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono",
            juce::jlimit(
                9.0F, 13.0F, static_cast<float>(button.getHeight()) * 0.34F),
            juce::Font::bold)));
        graphics.drawFittedText(
            button.getButtonText(), button.getLocalBounds().reduced(5, 2),
            juce::Justification::centred, 1, 0.72F);
    }
};

class WaveformComponent final : public juce::Component,
                                public juce::DragAndDropTarget
{
public:
    enum class EditMode
    {
        region,
        slice,
        blade
    };

    std::function<void(std::size_t, const juce::File&)> onFileDropped;
    std::function<void(std::size_t)> onSourceSelected;
    std::function<void(std::size_t, double, double)> onRangeSelected;
    std::function<void(std::size_t, double)> onBladeCut;

    void setPeaks(std::size_t sourceIndex,
                  std::vector<navalha::WaveformPeak> newPeaks)
    {
        if (sourceIndex >= peaks.size())
            return;
        peaks[sourceIndex] = std::move(newPeaks);
        repaint();
    }

    void setSlices(std::size_t sourceIndex,
                   std::span<const navalha::Slice> newSlices)
    {
        if (sourceIndex >= sliceBoundaries.size())
            return;
        auto& boundaries = sliceBoundaries[sourceIndex];
        boundaries.clear();
        if (!newSlices.empty())
        {
            boundaries.push_back(newSlices.front().start);
            for (const auto& slice : newSlices)
                boundaries.push_back(slice.end);
        }
        repaint();
    }

    void setSelectedSource(std::size_t sourceIndex)
    {
        selectedSource = std::min<std::size_t>(sourceIndex, 1);
        repaint();
    }

    void setEditMode(EditMode newMode)
    {
        editMode = newMode;
        repaint();
    }

    void setEditRange(std::size_t sourceIndex, double start, double end)
    {
        if (sourceIndex >= editRanges.size())
            return;
        editRanges[sourceIndex] = {
            std::clamp(std::min(start, end), 0.0, 1.0),
            std::clamp(std::max(start, end), 0.0, 1.0)};
        repaint();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(0xff080a0a));
        auto lanes = getLocalBounds();
        auto sourceA = lanes.removeFromTop(separatorY());
        const std::array sourceBounds {sourceA, lanes};
        for (std::size_t sourceIndex = 0;
             sourceIndex < sourceBounds.size(); ++sourceIndex)
        {
            auto lane = sourceBounds[sourceIndex].reduced(4, 3);
            const auto sourceAccent = sourceIndex == 0
                ? juce::Colour(Arcade::yellow)
                : juce::Colour(Arcade::red);
            const auto sourceAccentHigh = sourceIndex == 0
                ? juce::Colour(Arcade::yellowHigh)
                : juce::Colour(Arcade::red).brighter(0.42F);
            const auto dragTarget = dragActive
                && sourceForY(dragPositionY) == sourceIndex;
            graphics.setColour(
                dragTarget
                    ? sourceAccent.withAlpha(0.18F)
                    : sourceIndex == selectedSource
                    ? tintedPanelSurface(sourceAccent, 0.12F)
                    : juce::Colour(Arcade::surface));
            graphics.fillRect(lane);
            graphics.setColour(
                dragTarget
                    ? sourceAccentHigh
                    : sourceIndex == selectedSource
                    ? sourceAccent
                    : juce::Colour(Arcade::line).interpolatedWith(
                        sourceAccent, 0.30F));
            graphics.drawRect(
                lane, dragTarget || sourceIndex == selectedSource ? 2 : 1);
            const auto centre = static_cast<float>(lane.getCentreY());
            const auto amplitude =
                static_cast<float>(lane.getHeight()) * 0.38F;
            graphics.setColour(juce::Colour(Arcade::line));
            graphics.drawHorizontalLine(
                lane.getCentreY(),
                static_cast<float>(lane.getX()),
                static_cast<float>(lane.getRight()));

            const auto& sourcePeaks = peaks[sourceIndex];
            // Keep the drop-zone prompt on the same typography as the
            // SOURCE A/B lane label; the previous default Graphics font made
            // the two source fields look unrelated.
            graphics.setFont(juce::Font(juce::FontOptions(
                "DejaVu Sans Mono", 10.0F, juce::Font::bold)));
            if (sourcePeaks.empty())
            {
                graphics.setColour(juce::Colour(Arcade::muted));
                graphics.drawFittedText(
                    sourceIndex == 0
                        ? "DROP AUDIO FOR SOURCE A"
                        : "DROP AUDIO FOR SOURCE B",
                    lane.reduced(150, 10), juce::Justification::centred, 1);
            }
            else
            {
                for (std::size_t index = 0;
                     index < sourcePeaks.size(); ++index)
                {
                    const auto x = static_cast<float>(lane.getX())
                        + static_cast<float>(index) * lane.getWidth()
                            / static_cast<float>(sourcePeaks.size());
                    const auto& peak = sourcePeaks[index];
                    graphics.setColour(juce::Colour(0xffe8ebe8));
                    graphics.drawVerticalLine(
                        static_cast<int>(x),
                        centre - peak.maximumLeft * amplitude,
                        centre - peak.minimumLeft * amplitude);
                    graphics.setColour(
                        juce::Colour(Arcade::steel).withAlpha(0.72F));
                    graphics.drawVerticalLine(
                        static_cast<int>(x),
                        centre - peak.maximumRight * amplitude,
                        centre - peak.minimumRight * amplitude);
                }

                const auto selection = editRanges[sourceIndex];
                const auto selectionX = static_cast<float>(lane.getX())
                    + static_cast<float>(selection.start)
                        * static_cast<float>(lane.getWidth());
                const auto selectionRight = static_cast<float>(lane.getX())
                    + static_cast<float>(selection.end)
                        * static_cast<float>(lane.getWidth());
                const auto selectionWidth =
                    std::max(1.0F, selectionRight - selectionX);
                const auto selectionColour =
                    editMode == EditMode::slice
                        ? juce::Colour(0xff64d8ff)
                        : juce::Colour(Arcade::yellowHigh);
                graphics.setColour(selectionColour.withAlpha(
                    sourceIndex == selectedSource ? 0.16F : 0.07F));
                graphics.fillRect(
                    selectionX, static_cast<float>(lane.getY()),
                    selectionWidth, static_cast<float>(lane.getHeight()));
                graphics.setColour(selectionColour.withAlpha(
                    sourceIndex == selectedSource ? 0.90F : 0.34F));
                graphics.drawVerticalLine(
                    static_cast<int>(selectionX),
                    static_cast<float>(lane.getY()),
                    static_cast<float>(lane.getBottom()));
                graphics.drawVerticalLine(
                    static_cast<int>(selectionRight),
                    static_cast<float>(lane.getY()),
                    static_cast<float>(lane.getBottom()));

                graphics.setColour(sourceAccent);
                const auto& boundaries = sliceBoundaries[sourceIndex];
                for (std::size_t boundary = 0;
                     boundary < boundaries.size(); ++boundary)
                {
                    const auto x = static_cast<float>(lane.getX())
                        + static_cast<float>(
                            boundaries[boundary]
                            * static_cast<double>(lane.getWidth()));
                    graphics.drawVerticalLine(
                        static_cast<int>(x),
                        static_cast<float>(lane.getY()),
                        static_cast<float>(lane.getBottom()));
                    if (boundary + 1 < boundaries.size())
                        graphics.drawText(
                            juce::String(boundary),
                            static_cast<int>(x) + 3,
                            lane.getY() + 18, 38, 18,
                            juce::Justification::left);
                }
            }

            graphics.setFont(juce::Font(juce::FontOptions(
                "DejaVu Sans Mono", 10.0F, juce::Font::bold)));
            graphics.setColour(
                sourceIndex == selectedSource
                    ? sourceAccentHigh
                    : juce::Colour(Arcade::muted).interpolatedWith(
                        sourceAccent, 0.36F));
            graphics.drawFittedText(
                sourceIndex == 0 ? "SOURCE A" : "SOURCE B",
                lane.removeFromTop(24).removeFromLeft(110),
                juce::Justification::centred, 1);
        }
        const auto divider = separatorY();
        graphics.setColour(juce::Colour(Arcade::yellowHigh));
        graphics.fillRect(0, divider - 1, getWidth(), 3);
        graphics.setColour(juce::Colour(Arcade::background));
        graphics.fillRoundedRectangle(
            static_cast<float>(getWidth() - 206),
            static_cast<float>(divider - 10), 198.0F, 20.0F, 3.0F);
        graphics.setColour(juce::Colour(Arcade::yellowHigh));
        graphics.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 9.0F, juce::Font::bold)));
        graphics.drawFittedText(
            "DRAG TO RESIZE | 2x RESET",
            getWidth() - 202, divider - 9, 190, 18,
            juce::Justification::centred, 1);
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        setMouseCursor(
            std::abs(event.y - separatorY()) <= 8
                ? juce::MouseCursor::UpDownResizeCursor
                : juce::MouseCursor::CrosshairCursor);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        resizing = std::abs(event.y - separatorY()) <= 10;
        if (resizing)
            return;
        setSelectedSource(sourceForY(event.y));
        if (onSourceSelected)
            onSourceSelected(selectedSource);
        const auto position = normalizedX(event.x, selectedSource);
        if (editMode == EditMode::blade)
        {
            if (onBladeCut)
                onBladeCut(selectedSource, position);
            return;
        }
        selectionAnchor = position;
        editRanges[selectedSource] = {position, position};
        selecting = true;
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (resizing)
        {
            splitRatio = std::clamp(
                static_cast<double>(event.y)
                    / static_cast<double>(std::max(1, getHeight())),
                0.20, 0.80);
            repaint();
            return;
        }
        if (!selecting)
            return;
        const auto position = normalizedX(event.x, selectedSource);
        editRanges[selectedSource] = {
            std::min(selectionAnchor, position),
            std::max(selectionAnchor, position)};
        repaint();
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (selecting)
        {
            const auto position = normalizedX(event.x, selectedSource);
            auto start = std::min(selectionAnchor, position);
            auto end = std::max(selectionAnchor, position);
            if (end - start < 0.001)
                end = std::min(1.0, start + 0.001);
            editRanges[selectedSource] = {start, end};
            if (onRangeSelected)
                onRangeSelected(selectedSource, start, end);
        }
        selecting = false;
        resizing = false;
        repaint();
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        if (std::abs(event.y - separatorY()) <= 12)
        {
            splitRatio = 0.5;
            repaint();
        }
    }

    bool isInterestedInDragSource(const SourceDetails& details) override
    {
        const juce::File file(details.description.toString());
        return file.existsAsFile()
            && file.hasFileExtension("wav;wave;aif;aiff");
    }

    void itemDragEnter(const SourceDetails& details) override
    {
        dragActive = true;
        dragPositionY = details.localPosition.y;
        repaint();
    }

    void itemDragMove(const SourceDetails& details) override
    {
        dragPositionY = details.localPosition.y;
        repaint();
    }

    void itemDragExit(const SourceDetails&) override
    {
        dragActive = false;
        repaint();
    }

    void itemDropped(const SourceDetails& details) override
    {
        const auto sourceIndex = sourceForY(details.localPosition.y);
        dragActive = false;
        repaint();
        if (onFileDropped)
            onFileDropped(sourceIndex,
                          juce::File(details.description.toString()));
    }

private:
    [[nodiscard]] int separatorY() const noexcept
    {
        return std::clamp(
            static_cast<int>(std::lround(
                splitRatio * static_cast<double>(getHeight()))),
            1, std::max(1, getHeight() - 1));
    }

    [[nodiscard]] std::size_t sourceForY(int y) const noexcept
    {
        return y < separatorY() ? 0U : 1U;
    }

    [[nodiscard]] double normalizedX(
        int x, std::size_t sourceIndex) const noexcept
    {
        auto bounds = getLocalBounds();
        auto sourceA = bounds.removeFromTop(separatorY());
        auto lane = (sourceIndex == 0 ? sourceA : bounds).reduced(4, 3);
        return std::clamp(
            static_cast<double>(x - lane.getX())
                / static_cast<double>(std::max(1, lane.getWidth())),
            0.0, 1.0);
    }

    std::array<std::vector<navalha::WaveformPeak>, 2> peaks;
    std::array<std::vector<double>, 2> sliceBoundaries;
    std::array<navalha::Slice, 2> editRanges {
        navalha::Slice {0.0, 1.0}, navalha::Slice {0.0, 1.0}};
    EditMode editMode = EditMode::region;
    std::size_t selectedSource = 0;
    double splitRatio = 0.5;
    double selectionAnchor = 0.0;
    bool dragActive = false;
    bool resizing = false;
    bool selecting = false;
    int dragPositionY = 0;
};

class AudioLibraryList final : public juce::Component,
                               private juce::ListBoxModel
{
public:
    AudioLibraryList()
    {
        list.setModel(this);
        list.setRowHeight(54);
        list.setColour(
            juce::ListBox::backgroundColourId, juce::Colour(Arcade::surface));
        list.setColour(
            juce::ListBox::outlineColourId, juce::Colour(Arcade::line));
        list.setOutlineThickness(1);
        addAndMakeVisible(list);
    }

    ~AudioLibraryList() override
    {
        list.setModel(nullptr);
    }

    void setRootDirectory(const juce::File& newRoot)
    {
        root = newRoot;
        allFiles.clear();
        juce::Array<juce::File> found;
        root.findChildFiles(
            found, juce::File::findFiles, true,
            "*.wav;*.wave;*.aif;*.aiff");
        const auto maximumFiles = juce::jmin(512, found.size());
        allFiles.reserve(static_cast<std::size_t>(maximumFiles));
        for (int index = 0; index < maximumFiles; ++index)
            allFiles.push_back(found.getReference(index));
        std::sort(allFiles.begin(), allFiles.end(),
                  [] (const auto& left, const auto& right)
                  {
                      return left.getFileName().compareNatural(
                          right.getFileName(), true) < 0;
                  });
        rebuildVisibleFiles();
    }

    void setFilterText(const juce::String& newFilter)
    {
        filterText = newFilter.trim();
        rebuildVisibleFiles();
    }

    [[nodiscard]] int fileCount() const noexcept
    {
        return static_cast<int>(files.size());
    }

    [[nodiscard]] const juce::File& rootDirectory() const noexcept
    {
        return root;
    }

private:
    void rebuildVisibleFiles()
    {
        files.clear();
        for (const auto& file : allFiles)
        {
            const auto searchable =
                file.getFileName() + " " + file.getRelativePathFrom(root);
            if (filterText.isEmpty()
                || searchable.containsIgnoreCase(filterText))
                files.push_back(file);
        }
        list.updateContent();
        list.repaint();
    }

public:
    std::function<void(const juce::File&)> onSelection;
    std::function<void(const juce::File&)> onPreview;

    void resized() override
    {
        list.setBounds(getLocalBounds());
    }

private:
    class RowComponent final : public juce::Component
    {
    public:
        RowComponent()
        {
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        }

        void update(const juce::File& newFile,
                    const juce::File& newRoot,
                    bool newSelected,
                    std::function<void()> selectAction,
                    std::function<void()> previewAction)
        {
            file = newFile;
            rootDirectory = newRoot;
            selected = newSelected;
            onSelect = std::move(selectAction);
            onPreview = std::move(previewAction);
            repaint();
        }

        void paint(juce::Graphics& graphics) override
        {
            auto bounds = getLocalBounds();
            graphics.fillAll(selected
                ? juce::Colour(Arcade::surfaceHigh).brighter(0.12F)
                : hovered ? juce::Colour(Arcade::surfaceHigh)
                          : juce::Colour(Arcade::surface));
            graphics.setColour(juce::Colour(Arcade::line));
            graphics.drawHorizontalLine(
                getHeight() - 1, 6.0F, static_cast<float>(getWidth() - 6));

            auto textBounds = bounds.reduced(7, 3);
            graphics.setColour(selected
                ? juce::Colour(Arcade::yellowHigh)
                : juce::Colour(Arcade::ink));
            graphics.setFont(juce::Font(
                juce::FontOptions("DejaVu Sans Mono", 12.0F, juce::Font::bold)));
            graphics.drawFittedText(
                file.getFileName(), textBounds.removeFromTop(22),
                juce::Justification::centredLeft, 1, 0.70F);
            graphics.setColour(juce::Colour(Arcade::muted));
            graphics.setFont(juce::Font(
                juce::FontOptions("DejaVu Sans Mono", 9.2F, juce::Font::plain)));
            const auto parentName = file.getParentDirectory() == rootDirectory
                ? rootDirectory.getFileName()
                : file.getParentDirectory().getFileName();
            const auto details = parentName + juce::String::fromUTF8(" · ")
                + file.getFileExtension().trimCharactersAtStart(".").toUpperCase()
                + juce::String::fromUTF8(" · ")
                + juce::File::descriptionOfSizeInBytes(file.getSize());
            auto detailsBounds = textBounds;
            auto dragChip = detailsBounds.removeFromRight(48).reduced(3, 3);
            graphics.drawFittedText(
                details, detailsBounds,
                juce::Justification::centredLeft, 1, 0.75F);

            graphics.setColour(
                hovered
                    ? juce::Colour(Arcade::yellow).withAlpha(0.18F)
                    : juce::Colour(Arcade::surfaceHigh));
            graphics.fillRoundedRectangle(dragChip.toFloat(), 2.0F);
            graphics.setColour(hovered
                ? juce::Colour(Arcade::yellowHigh)
                : juce::Colour(Arcade::yellow));
            graphics.drawRoundedRectangle(dragChip.toFloat(), 2.0F, 1.0F);
            graphics.setFont(juce::Font(juce::FontOptions(
                "DejaVu Sans Mono", 8.0F, juce::Font::bold)));
            graphics.drawFittedText(
                "DRAG", dragChip, juce::Justification::centred, 1);
        }

        void mouseEnter(const juce::MouseEvent&) override
        {
            hovered = true;
            repaint();
        }

        void mouseExit(const juce::MouseEvent&) override
        {
            hovered = false;
            repaint();
        }

        void mouseDown(const juce::MouseEvent&) override
        {
            dragStarted = false;
            selected = true;
            if (onSelect)
                onSelect();
            repaint();
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (dragStarted || event.getDistanceFromDragStart() < 5)
                return;
            if (auto* container =
                    findParentComponentOfClass<juce::DragAndDropContainer>())
            {
                dragStarted = true;
                container->startDragging(file.getFullPathName(), this);
            }
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            dragStarted = false;
        }

        void mouseDoubleClick(const juce::MouseEvent&) override
        {
            if (onPreview)
                onPreview();
        }

    private:
        juce::File file;
        juce::File rootDirectory;
        std::function<void()> onSelect;
        std::function<void()> onPreview;
        bool selected = false;
        bool hovered = false;
        bool dragStarted = false;
    };

    int getNumRows() override
    {
        return static_cast<int>(files.size());
    }

    juce::Component* refreshComponentForRow(
        int row, bool selected, juce::Component* existing) override
    {
        auto* rowComponent = dynamic_cast<RowComponent*>(existing);
        if (rowComponent == nullptr)
            rowComponent = new RowComponent();
        if (juce::isPositiveAndBelow(row, static_cast<int>(files.size())))
        {
            const auto file = files[static_cast<std::size_t>(row)];
            rowComponent->update(
                file, root, selected,
                [this, row, file]
                {
                    list.selectRow(row);
                    if (onSelection)
                        onSelection(file);
                },
                [this, row, file]
                {
                    list.selectRow(row);
                    if (onSelection)
                        onSelection(file);
                    if (onPreview)
                        onPreview(file);
                });
        }
        return rowComponent;
    }

    void paintListBoxItem(int row,
                          juce::Graphics& graphics,
                          int width,
                          int height,
                          bool selected) override
    {
        if (!juce::isPositiveAndBelow(row, static_cast<int>(files.size())))
            return;

        auto bounds = juce::Rectangle<int>(0, 0, width, height);
        graphics.fillAll(selected
            ? juce::Colour(Arcade::surfaceHigh).brighter(0.12F)
            : juce::Colour(Arcade::surface));
        graphics.setColour(juce::Colour(Arcade::line));
        graphics.drawHorizontalLine(
            height - 1, 6.0F, static_cast<float>(width - 6));

        graphics.setColour(juce::Colour(Arcade::yellow));
        graphics.fillRect(bounds.removeFromRight(3).reduced(0, 8));

        const auto& file = files[static_cast<std::size_t>(row)];
        auto textBounds = bounds.reduced(7, 3);
        graphics.setColour(selected
            ? juce::Colour(Arcade::yellowHigh)
            : juce::Colour(Arcade::ink));
        graphics.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 12.5F, juce::Font::bold)));
        graphics.drawText(
            file.getFileName(), textBounds.removeFromTop(22),
            juce::Justification::centredLeft, true);
        graphics.setColour(juce::Colour(Arcade::muted));
        graphics.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 9.0F, juce::Font::plain)));
        graphics.drawFittedText(
            file.getRelativePathFrom(root), textBounds,
            juce::Justification::centredLeft, 1);
    }

    juce::File root;
    juce::String filterText;
    std::vector<juce::File> allFiles;
    std::vector<juce::File> files;
    juce::ListBox list;
};

class AudioSettingsPanel final : public juce::Component,
                                 private juce::ChangeListener
{
public:
    explicit AudioSettingsPanel(juce::AudioDeviceManager& manager)
        : deviceManager(manager),
          selector(manager, 0, 0, 2, 2, false, false, true, false)
    {
        setLookAndFeel(&lookAndFeel);
        heading.setText("AUDIO SETUP", juce::dontSendNotification);
        heading.setJustificationType(juce::Justification::centredLeft);
        heading.getProperties().set("arcadeTitle", true);
        heading.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        addAndMakeVisible(heading);

        subtitle.setText(
            juce::String::fromUTF8(
                "OUTPUT DEVICE · SAMPLE RATE · BUFFER"),
            juce::dontSendNotification);
        subtitle.setJustificationType(juce::Justification::centredLeft);
        subtitle.getProperties().set("arcadeFontSize", 10.0);
        subtitle.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        addAndMakeVisible(subtitle);

        deviceStatus.setJustificationType(juce::Justification::centredLeft);
        deviceStatus.getProperties().set("arcadeFontSize", 11.0);
        deviceStatus.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::ink));
        addAndMakeVisible(deviceStatus);

        formatStatus.setJustificationType(juce::Justification::centredRight);
        formatStatus.getProperties().set("arcadeFontSize", 11.0);
        formatStatus.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellow));
        addAndMakeVisible(formatStatus);

        addAndMakeVisible(selector);

        safetyNote.setText(
            "Start with low monitor volume. 512 samples is the safe first "
            "test; use 128/256 only after stable playback.",
            juce::dontSendNotification);
        safetyNote.setJustificationType(juce::Justification::centredLeft);
        safetyNote.getProperties().set("arcadeFontSize", 10.0);
        safetyNote.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        addAndMakeVisible(safetyNote);

        // The global LEARN listener can therefore explain controls created
        // internally by JUCE's AudioDeviceSelectorComponent as well.
        getProperties().set("learnKey", "audio");
        deviceManager.addChangeListener(this);
        refreshDeviceStatus();
        setSize(760, 640);
    }

    ~AudioSettingsPanel() override
    {
        deviceManager.removeChangeListener(this);
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(Arcade::background));

        auto bounds = getLocalBounds().reduced(10);
        auto header = bounds.removeFromTop(70).toFloat();
        juce::ColourGradient glow(
            juce::Colour(0x18ffd84a), header.getCentreX(), header.getY(),
            juce::Colours::transparentBlack, header.getCentreX(),
            header.getBottom(), false);
        graphics.setGradientFill(glow);
        graphics.fillRoundedRectangle(header, 4.0F);
        graphics.setColour(juce::Colour(Arcade::yellow));
        graphics.fillRect(header.getX(), header.getY(), 5.0F, header.getHeight());

        auto status = bounds.removeFromTop(54).reduced(0, 4).toFloat();
        graphics.setColour(juce::Colour(Arcade::surfaceHigh));
        graphics.fillRoundedRectangle(status, 4.0F);
        graphics.setColour(juce::Colour(Arcade::line));
        graphics.drawRoundedRectangle(status, 4.0F, 1.0F);

        auto footer = getLocalBounds().reduced(10).removeFromBottom(52).toFloat();
        graphics.setColour(juce::Colour(Arcade::surface));
        graphics.fillRoundedRectangle(footer, 4.0F);
        graphics.setColour(juce::Colour(Arcade::line));
        graphics.drawRoundedRectangle(footer, 4.0F, 1.0F);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(18, 12);
        auto header = area.removeFromTop(68);
        heading.setBounds(header.removeFromTop(39).reduced(10, 0));
        subtitle.setBounds(header.reduced(10, 0));

        auto status = area.removeFromTop(54).reduced(10, 7);
        deviceStatus.setBounds(status.removeFromLeft(
            static_cast<int>(status.getWidth() * 0.62F)));
        formatStatus.setBounds(status);

        area.removeFromTop(6);
        auto footer = area.removeFromBottom(48);
        safetyNote.setBounds(footer.reduced(10, 2));
        area.removeFromBottom(6);
        selector.setBounds(area.reduced(4, 0));
    }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        refreshDeviceStatus();
    }

    void refreshDeviceStatus()
    {
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            deviceStatus.setText(
                "OUTPUT | " + device->getName(),
                juce::dontSendNotification);
            formatStatus.setText(
                juce::String(device->getCurrentSampleRate(), 0) + " Hz | "
                    + juce::String(device->getCurrentBufferSizeSamples())
                    + " SAMPLES | STEREO",
                juce::dontSendNotification);
        }
        else
        {
            deviceStatus.setText(
                "OUTPUT | NO ACTIVE DEVICE",
                juce::dontSendNotification);
            formatStatus.setText(
                "SELECT AN OUTPUT BELOW",
                juce::dontSendNotification);
        }
    }

    ArcadeLookAndFeel lookAndFeel;
    juce::AudioDeviceManager& deviceManager;
    juce::AudioDeviceSelectorComponent selector;
    juce::Label heading;
    juce::Label subtitle;
    juce::Label deviceStatus;
    juce::Label formatStatus;
    juce::Label safetyNote;
};

class PagedViewport final : public juce::Viewport
{
public:
    std::function<void(juce::Rectangle<int>)> onVisibleAreaChanged;

    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override
    {
        juce::Viewport::mouseWheelMove(event, wheel);
    }

private:
    void visibleAreaChanged(const juce::Rectangle<int>& newVisibleArea) override
    {
        if (onVisibleAreaChanged)
            onVisibleAreaChanged(newVisibleArea);
    }
};

class ControlTracePad final : public juce::Component
{
public:
    std::function<void()> onGestureStart;
    std::function<void(double, int)> onMove;
    std::function<void()> onGestureEnd;

    ControlTracePad()
    {
        setWantsKeyboardFocus(true);
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
        setTitle("XY MOD - horizontal BPM, vertical pitch");
        setDescription(
            "Drag horizontally for BPM and vertically for pitch. "
            "Pitch snaps to 24 semitone positions.");
    }

    void setValues(double newBpm, int newPitch)
    {
        bpm = std::clamp(newBpm, 20.0, 400.0);
        pitch = std::clamp(newPitch, -12, 11);
        repaint();
    }

    void setTrace(std::span<const navalha::ControlTracePoint> newPoints)
    {
        trace.assign(newPoints.begin(), newPoints.end());
        repaint();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(Arcade::background));
        const auto pad = getLocalBounds().toFloat().reduced(1.0F);
        graphics.setColour(juce::Colour(Arcade::surface));
        graphics.fillRect(pad);
        graphics.setColour(juce::Colour(Arcade::line));
        graphics.drawRect(pad, 1.0F);

        for (int divisionIndex = 1; divisionIndex < 4; ++divisionIndex)
        {
            const auto x = pad.getX()
                + pad.getWidth() * static_cast<float>(divisionIndex) / 4.0F;
            const auto y = pad.getY()
                + pad.getHeight() * static_cast<float>(divisionIndex) / 4.0F;
            graphics.drawVerticalLine(
                static_cast<int>(x), pad.getY(), pad.getBottom());
            graphics.drawHorizontalLine(
                static_cast<int>(y), pad.getX(), pad.getRight());
        }

        const auto zeroY = pointFor(120.0, 0, pad).y;
        graphics.setColour(juce::Colour(Arcade::yellow).withAlpha(0.22F));
        graphics.drawHorizontalLine(
            static_cast<int>(zeroY), pad.getX(), pad.getRight());

        if (trace.size() >= 2)
        {
            juce::Path path;
            const auto first = pointFor(trace.front().bpm, trace.front().pitch, pad);
            path.startNewSubPath(first);
            for (std::size_t index = 1; index < trace.size(); ++index)
                path.lineTo(pointFor(trace[index].bpm, trace[index].pitch, pad));
            graphics.setColour(
                juce::Colour(Arcade::yellowHigh).withAlpha(0.56F));
            graphics.strokePath(
                path, juce::PathStrokeType(
                    2.0F, juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));
        }

        const auto currentPoint = pointFor(bpm, pitch, pad);
        graphics.setColour(juce::Colour(Arcade::yellow).withAlpha(0.32F));
        graphics.drawVerticalLine(
            static_cast<int>(currentPoint.x), pad.getY(), pad.getBottom());
        graphics.drawHorizontalLine(
            static_cast<int>(currentPoint.y), pad.getX(), pad.getRight());
        graphics.setColour(juce::Colour(Arcade::yellowHigh));
        graphics.fillEllipse(
            currentPoint.x - 6.0F, currentPoint.y - 6.0F, 12.0F, 12.0F);

        graphics.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 10.5F, juce::Font::bold)));
        graphics.drawFittedText(
            "BPM " + juce::String(std::lround(bpm))
                + " | PITCH " + signedPitch(pitch),
            getLocalBounds().reduced(8, 5),
            juce::Justification::topLeft, 1);
        graphics.setColour(juce::Colour(Arcade::muted));
        graphics.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 8.5F, juce::Font::plain)));
        graphics.drawFittedText(
            "20 BPM", getLocalBounds().reduced(7, 4),
            juce::Justification::bottomLeft, 1);
        graphics.drawFittedText(
            "400 BPM", getLocalBounds().reduced(7, 4),
            juce::Justification::bottomRight, 1);
        graphics.drawFittedText(
            "+11 st", getLocalBounds().reduced(7, 4),
            juce::Justification::topRight, 1);
        graphics.drawFittedText(
            "-12 st", getLocalBounds().reduced(7, 4),
            juce::Justification::bottom, 1);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        grabKeyboardFocus();
        dragging = true;
        if (onGestureStart)
            onGestureStart();
        moveTo(event.position);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        moveTo(event.position);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (!dragging)
            return;
        moveTo(event.position);
        dragging = false;
        if (onGestureEnd)
            onGestureEnd();
    }

private:
    static juce::String signedPitch(int value)
    {
        return juce::String(value >= 0 ? "+" : "") + juce::String(value);
    }

    static juce::Point<float> pointFor(
        double pointBpm, int pointPitch, juce::Rectangle<float> bounds)
    {
        return {
            bounds.getX() + static_cast<float>(
                (std::clamp(pointBpm, 20.0, 400.0) - 20.0) / 380.0)
                * bounds.getWidth(),
            bounds.getY() + static_cast<float>(
                (11.0 - static_cast<double>(
                    std::clamp(pointPitch, -12, 11))) / 23.0)
                * bounds.getHeight()};
    }

    void moveTo(juce::Point<float> point)
    {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0F);
        const auto normalizedX = std::clamp(
            static_cast<double>(point.x - bounds.getX())
                / std::max(1.0F, bounds.getWidth()),
            0.0, 1.0);
        const auto normalizedY = std::clamp(
            static_cast<double>(point.y - bounds.getY())
                / std::max(1.0F, bounds.getHeight()),
            0.0, 1.0);
        bpm = 20.0 + normalizedX * 380.0;
        pitch = std::clamp(
            static_cast<int>(std::lround(11.0 - normalizedY * 23.0)),
            -12, 11);
        repaint();
        if (onMove)
            onMove(bpm, pitch);
    }

    std::vector<navalha::ControlTracePoint> trace;
    double bpm = 120.0;
    int pitch = 0;
    bool dragging = false;
};

enum class PatternMacro
{
    randomA,
    randomB,
    randomAB,
    interleave,
    forward,
    reverse,
    zero,
    gap
};

struct PerformanceSnapshot
{
    bool running = false;
    bool recording = false;
    std::size_t step = 0;
    double bpm = 120.0;
    int pitch = 0;
    std::size_t pattern = 0;
    std::size_t source = 0;
    navalha::TimingMode timing = navalha::TimingMode::grid;
    double master = 0.8;
    double balance = 0.0;
    bool assisted = false;
    bool repeat = true;
    bool formEnabled = false;
    bool formHold = false;
    std::size_t formScene = 0;
};

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer,
                            private juce::ChangeListener,
                            private juce::FocusChangeListener
{
public:
    MainComponent()
        : engine(session)
    {
        arcadeLogo = juce::Drawable::createFromImageData(
            BinaryData::navalha2headerarcade_svg,
            BinaryData::navalha2headerarcade_svgSize);
        title.setText({}, juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.getProperties().set("arcadeTitle", true);
        title.setColour(juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        addAndMakeVisible(title);

        configureButton(play, "PLAY", [this]
        {
            if (!engine.submitCommand({navalha::EngineCommandType::start}))
                showStatus("COMMAND QUEUE FULL");
        });
        play.getProperties().set("arcadeAccent", "play");
        configureButton(stop, "STOP", [this]
        {
            if (!engine.submitCommand({navalha::EngineCommandType::stop}))
                showStatus("COMMAND QUEUE FULL");
        });
        stop.getProperties().set("arcadeAccent", "stop");
        configureButton(resetTransport, "RESET", [this]
        {
            submitOrWarn({navalha::EngineCommandType::reset});
            transportElapsedMilliseconds = 0.0;
            transportStartedAtMilliseconds =
                juce::Time::getMillisecondCounterHiRes();
            transportClock.setText("00:00:00", juce::dontSendNotification);
            showStatus("TRANSPORT RESET");
        });
        configureButton(openProject, "OPEN PROJECT", [this] { chooseProjectToOpen(); });
        configureButton(saveProject, "SAVE PROJECT", [this] { chooseProjectToSave(); });
        configureButton(savePortable, "SAVE PORTABLE", [this] { choosePortableToSave(); });
        savePortable.setTooltip(
            "Save a self-contained Project v2 ZIP with copies of SOURCE A/B. "
            "This does not render the performance.");
        configureButton(record, "REC", [this]
        {
            if (recorder.isRunning())
                finalizeRecording();
            else
                chooseRecordingPath();
        });
        record.getProperties().set("arcadeAccent", "record");
        transportClock.setText("00:00:00", juce::dontSendNotification);
        transportClock.setJustificationType(juce::Justification::centred);
        transportClock.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::background));
        transportClock.setColour(
            juce::Label::outlineColourId, juce::Colour(Arcade::line));
        transportClock.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        transportClock.getProperties().set("arcadeClock", true);
        addAndMakeVisible(transportClock);

        master.setRange(0.0, 1.0, 0.01);
        master.setValue(session.masterLevel, juce::dontSendNotification);
        master.onValueChange = [this]
        {
            static_cast<void>(engine.submitCommand({
                navalha::EngineCommandType::setMasterLevel,
                0, 0, master.getValue()}));
        };
        addAndMakeVisible(master);
        masterLabel.setText("MASTER CREATIVE", juce::dontSendNotification);
        masterLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(masterLabel);

        outputTrim.setRange(-24.0, 0.0, 0.1);
        outputTrim.setValue(0.0, juce::dontSendNotification);
        outputTrim.setTextValueSuffix(" dB");
        outputTrim.setTooltip(
            "Technical attenuation before the safety limiter; independent "
            "from the creative MASTER.");
        outputTrim.onValueChange = [this]
        {
            static_cast<void>(engine.setOutputTrimDb(
                static_cast<float>(outputTrim.getValue())));
        };
        outputTrim.onDragEnd = [this] { saveAudioSettings(); };
        addAndMakeVisible(outputTrim);
        outputTrimLabel.setText("OUTPUT TRIM", juce::dontSendNotification);
        outputTrimLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(outputTrimLabel);
        outputMute.setButtonText("MUTE");
        outputMute.setTooltip("Click-free technical mute after MASTER.");
        outputMute.onClick = [this]
        {
            engine.setOutputMuted(outputMute.getToggleState());
            saveAudioSettings();
            showStatus(outputMute.getToggleState()
                ? "OUTPUT MUTED" : "OUTPUT FADE IN");
        };
        addAndMakeVisible(outputMute);

        configureParameterLabel(tempoLabel, "BPM");
        tempo.setRange(20.0, 400.0, 1.0);
        tempo.setValue(session.sequencer.tempo(), juce::dontSendNotification);
        tempo.onValueChange = [this]
        {
            submitOrWarn({
                navalha::EngineCommandType::setTempo,
                static_cast<std::size_t>(division.getSelectedItemIndex()),
                0,
                tempo.getValue()});
            captureTracePoint();
        };
        addAndMakeVisible(tempo);

        configureParameterLabel(divisionLabel, "RATE");
        division.addItemList({"1x", "2x", "3x", "4x"}, 1);
        division.setSelectedItemIndex(
            static_cast<int>(session.sequencer.division()), juce::dontSendNotification);
        division.onChange = [this]
        {
            submitOrWarn({
                navalha::EngineCommandType::setTempo,
                static_cast<std::size_t>(division.getSelectedItemIndex()),
                0,
                tempo.getValue()});
        };
        addAndMakeVisible(division);

        configureParameterLabel(patternLabel, "PATTERN");
        pattern.addItemList(
            {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10"}, 1);
        pattern.setSelectedItemIndex(
            static_cast<int>(session.sequencer.currentPattern()),
            juce::dontSendNotification);
        pattern.onChange = [this]
        {
            submitOrWarn({
                navalha::EngineCommandType::selectPattern,
                static_cast<std::size_t>(pattern.getSelectedItemIndex())});
            refreshPatternCells();
            syncGestureControls();
        };
        addAndMakeVisible(pattern);

        configureParameterLabel(timingLabel, "TIMING");
        timing.addItemList({"GRID", "FREE", "JITTER"}, 1);
        timing.setSelectedItemIndex(
            static_cast<int>(session.sequencer.timing()), juce::dontSendNotification);
        timing.onChange = [this]
        {
            submitTiming();
        };
        addAndMakeVisible(timing);

        configureParameterLabel(jitterLabel, "JITTER %");
        jitterControl.setRange(0.0, 40.0, 0.1);
        jitterControl.setValue(jitterAmount, juce::dontSendNotification);
        jitterControl.onValueChange = [this]
        {
            jitterAmount = jitterControl.getValue();
            submitTiming();
        };
        addAndMakeVisible(jitterControl);
        configureParameterLabel(timingSeedLabel, "TIMING SEED");
        timingSeedEditor.setText(
            navalha::AssistedRng::formatSeed(timingSeed), false);
        timingSeedEditor.setSelectAllWhenFocused(true);
        styleEditableTextField(timingSeedEditor);
        addAndMakeVisible(timingSeedEditor);
        configureButton(applyTimingSeed, "APPLY", [this] { updateTimingSeed(); });
        transportInfo.setJustificationType(juce::Justification::centred);
        transportInfo.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surface));
        transportInfo.setColour(
            juce::Label::outlineColourId, juce::Colour(Arcade::line));
        transportInfo.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        transportInfo.setText("TRANSPORT: STOP", juce::dontSendNotification);
        addAndMakeVisible(transportInfo);

        configureParameterLabel(pitchLabel, "PITCH");
        pitchSemitones.setRange(-12.0, 11.0, 1.0);
        pitchSemitones.setTextValueSuffix(" st");
        pitchSemitones.setValue(
            session.heritagePitchSemitones, juce::dontSendNotification);
        pitchSemitones.onValueChange = [this] { submitPitch(); };
        addAndMakeVisible(pitchSemitones);

        configureParameterLabel(pitchMixLabel, "HERITAGE");
        pitchMix.setRange(0.0, 1.0, 0.01);
        pitchMix.setValue(session.heritagePitchMode, juce::dontSendNotification);
        pitchMix.onValueChange = [this] { submitPitch(); };
        addAndMakeVisible(pitchMix);
        configureButton(pitchBypass, "HERITAGE OFF", [this]
        {
            const auto enabled = pitchMix.getValue() > 0.0001;
            pitchMix.setValue(enabled ? 0.0 : 1.0, juce::sendNotification);
        });
        pitchBypass.setTooltip(
            "Toggle Heritage Pitch between bypass and full legacy mode. "
            "Use the HERITAGE slider for intermediate blends.");
        configureButton(pitchZero, "0", [this]
        {
            pitchSemitones.setValue(0.0, juce::sendNotification);
        });
        configureButton(pitchAudition, "AUDITION", [this]
        {
            submitOrWarn({
                navalha::EngineCommandType::triggerSlice,
                selectedSliceSource(), selectedSliceIndex()});
            showStatus("HERITAGE PITCH AUDITION");
        });
        pitchBypass.getProperties().set("arcadeLargeButton", true);
        pitchAudition.getProperties().set("arcadeLargeButton", true);
        updatePitchModeButtons();

        configureParameterLabel(patternCellsLabel, "STEPS");
        for (std::size_t step = 0; step < patternCells.size(); ++step)
        {
            auto& cell = patternCells[step];
            cell.setRange(0.0, static_cast<double>(navalha::gapCellCode), 1.0);
            cell.setSliderStyle(juce::Slider::LinearBar);
            cell.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 24);
            cell.textFromValueFunction = [] (double rawValue)
            {
                const auto code = static_cast<unsigned int>(std::lround(rawValue));
                if (code == navalha::gapCellCode)
                    return juce::String("GAP");
                if (code >= 128U)
                    return juce::String("B") + juce::String(code - 128U);
                return juce::String("A") + juce::String(code);
            };
            cell.onValueChange = [this, step]
            {
                const auto patternIndex =
                    static_cast<std::size_t>(pattern.getSelectedItemIndex());
                const auto code = static_cast<std::uint16_t>(
                    std::lround(patternCells[step].getValue()));
                uiPatterns.setCell(patternIndex, step, code);
                submitOrWarn({
                    navalha::EngineCommandType::setPatternCell,
                    patternIndex,
                    step,
                    static_cast<double>(code)});
            };
            addAndMakeVisible(cell);
        }
        refreshPatternCells();

        configureParameterLabel(orderLabel, "ORDER");
        configureButton(randomA, "RANDOM A", [this]
        {
            applyPatternMacro(PatternMacro::randomA);
        });
        configureButton(randomB, "RANDOM B", [this]
        {
            applyPatternMacro(PatternMacro::randomB);
        });
        configureButton(randomAB, "RANDOM A+B", [this]
        {
            applyPatternMacro(PatternMacro::randomAB);
        });
        configureButton(interleave, "INTERLEAVE", [this]
        {
            applyPatternMacro(PatternMacro::interleave);
        });
        configureButton(forwardOrder, juce::String::fromUTF8("0→7"), [this]
        {
            applyPatternMacro(PatternMacro::forward);
        });
        configureButton(reverseOrder, juce::String::fromUTF8("7→0"), [this]
        {
            applyPatternMacro(PatternMacro::reverse);
        });
        configureButton(zeroOrder, "ZERO", [this]
        {
            applyPatternMacro(PatternMacro::zero);
        });
        configureButton(gapOrder, "GAP", [this]
        {
            applyPatternMacro(PatternMacro::gap);
        });

        configureParameterLabel(gestureLabel, "GESTURES");
        gestureStep.addItemList(
            {"STEP 1", "STEP 2", "STEP 3", "STEP 4",
             "STEP 5", "STEP 6", "STEP 7", "STEP 8"}, 1);
        gestureStep.setSelectedItemIndex(0, juce::dontSendNotification);
        gestureStep.onChange = [this] { updateMemoryButton(); };
        addAndMakeVisible(gestureStep);
        configureButton(memoryToggle, "MEMORY", [this] { toggleMemory(); });
        for (auto* slider : {
                 &mutationAmount, &erosionAmount, &deconstructAmount})
        {
            slider->setRange(0.0, 100.0, 1.0);
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(
                juce::Slider::TextBoxRight, false, 48, 22);
            slider->onValueChange = [this] { applyStructuralTransform(); };
            addAndMakeVisible(*slider);
        }
        mutationAmount.setName("Mutation amount");
        erosionAmount.setName("Erosion amount");
        deconstructAmount.setName("Deconstruct amount");
        configureButton(commitTransform, "COMMIT", [this] { commitTransformState(); });
        configureButton(restoreTransform, "RESTORE", [this] { restoreTransformState(); });
        configureButton(stutter, "STUTTER x4", [this] { startStutter(); });
        configureButton(burst, "BURST x8", [this]
        {
            submitOrWarn({
                navalha::EngineCommandType::startBurst,
                selectedSliceSource()});
            showStatus("BURST x8");
        });
        configureButton(micro, "MICRO x8", [this] { appendMicroSlices(); });
        configureButton(reverseSlice, "REVERSE", [this]
        {
            submitOrWarn({
                navalha::EngineCommandType::triggerSlice,
                selectedSliceSource(), selectedSliceIndex(), 1.0});
            showStatus("REVERSE SLICE");
        });

        configureParameterLabel(formLabel, "FORM");
        formScene.addItemList(
            {"INTRO", "DEVELOPMENT", "RUPTURE", "CLIMAX", "EXIT"}, 1);
        formScene.onChange = [this]
        {
            const auto index = static_cast<std::size_t>(
                std::max(0, formScene.getSelectedItemIndex()));
            static_cast<void>(uiFormDirector.selectScene(index));
            submitOrWarn({
                navalha::EngineCommandType::selectFormScene, index});
            syncFormControls();
        };
        addAndMakeVisible(formScene);
        configureButton(formEnable, "ARM FORM", [this]
        {
            const auto enabled = !uiFormDirector.state().enabled;
            uiFormDirector.setEnabled(enabled);
            submitOrWarn({
                navalha::EngineCommandType::setFormEnabled,
                static_cast<std::size_t>(enabled)});
            syncFormControls();
        });
        configureButton(formHold, "HOLD", [this]
        {
            uiFormDirector.toggleHold();
            submitOrWarn({navalha::EngineCommandType::toggleFormHold});
            syncFormControls();
        });
        configureButton(formNext, "NEXT", [this]
        {
            static_cast<void>(uiFormDirector.advanceScene());
            submitOrWarn({navalha::EngineCommandType::advanceFormScene});
            syncFormControls();
        });
        configureButton(formReset, "RESET", [this]
        {
            uiFormDirector.reset();
            submitOrWarn({navalha::EngineCommandType::resetFormDirector});
            syncFormControls();
        });
        for (auto* slider : {&formBars, &formEnergy, &formVariation})
        {
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(
                juce::Slider::TextBoxRight, false, 48, 22);
            slider->onValueChange = [this] { editFormScene(); };
            addAndMakeVisible(*slider);
        }
        formBars.setRange(1.0, 128.0, 1.0);
        formEnergy.setRange(0.0, 100.0, 1.0);
        formVariation.setRange(0.0, 100.0, 1.0);
        syncFormControls();

        configureParameterLabel(assistedLabel, "ASSISTED");
        for (auto* toggle : {
                 &assistedEnable, &assistedRepeat,
                 &assistedSource, &assistedOrder,
                 &assistedRegion, &assistedCuts, &assistedMix,
                 &assistedTransform, &assistedGaps, &assistedPitch,
                 &assistedFragments})
        {
            toggle->onClick = [this] { submitAssistedSettings(); };
            addAndMakeVisible(*toggle);
        }
        assistedEnable.setButtonText("AUTO");
        assistedRepeat.setButtonText("REPEAT");
        assistedSource.setButtonText("SOURCE");
        assistedOrder.setButtonText("PATTERN");
        assistedRegion.setButtonText("REGION");
        assistedCuts.setButtonText("CUTS");
        assistedMix.setButtonText("MIX");
        assistedTransform.setButtonText("TRANSFORM");
        assistedGaps.setButtonText("GAPS");
        assistedPitch.setButtonText("PITCH");
        assistedFragments.setButtonText("FRAGMENTS");
        configureButton(assistedNext, "NEXT", [this]
        {
            submitOrWarn({navalha::EngineCommandType::forceAssistedDecision});
            showStatus("ASSISTED | NEXT DECISION");
        });
        configureButton(assistedKeep, "KEEP", [this]
        {
            submitOrWarn({navalha::EngineCommandType::keepAssistedCuts});
            showStatus("ASSISTED CUTS KEPT");
        });
        configureButton(assistedRestore, "RESTORE", [this]
        {
            submitOrWarn({navalha::EngineCommandType::restoreAssistedCuts});
            showStatus("ASSISTED CUTS RESTORED");
        });
        for (auto* toggle : {
                 &lockSource, &lockCuts, &lockPattern, &lockTransform,
                 &lockPitch, &lockGap, &lockMix, &lockVoices})
        {
            toggle->onClick = [this] { submitMotifLocks(); };
            addAndMakeVisible(*toggle);
        }
        lockSource.setButtonText("L:SRC");
        lockCuts.setButtonText("L:CUT");
        lockPattern.setButtonText("L:PTN");
        lockTransform.setButtonText("L:XFM");
        lockPitch.setButtonText("L:PIT");
        lockGap.setButtonText("L:GAP");
        lockMix.setButtonText("L:MIX");
        lockVoices.setButtonText("L:VOX");
        configureParameterLabel(motifLabel, "MOTIF MEMORY");
        for (std::size_t slot = 0; slot < motifSlotButtons.size(); ++slot)
            configureButton(
                motifSlotButtons[slot], juce::String(slot + 1),
                [this, slot]
                {
                    selectedMotifSlot = slot;
                    syncMotifControls();
                });
        motifName.setTextToShowWhenEmpty("MOTIF NAME", juce::Colour(Arcade::muted));
        motifName.setSelectAllWhenFocused(true);
        styleEditableTextField(motifName);
        motifName.onTextChange = [this]
        {
            if (syncingMotifControls)
                return;
            auto& slot = uiMotifSlots[selectedMotifSlot];
            if (slot.occupied)
                slot.name = motifName.getText().trim().substring(0, 28)
                    .toStdString();
        };
        addAndMakeVisible(motifName);
        configureButton(motifCapture, "CAPTURE", [this] { captureMotif(); });
        configureButton(motifRecall, "RECALL", [this] { recallMotif(false); });
        configureButton(motifVary, "VARY", [this] { recallMotif(true); });
        configureButton(motifDelete, "DELETE", [this] { deleteMotif(); });
        motifInfo.setJustificationType(juce::Justification::centredLeft);
        motifInfo.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        addAndMakeVisible(motifInfo);
        syncMotifControls();
        for (auto* slider : {
                 &assistedMinBpm, &assistedMaxBpm, &assistedVariation})
        {
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(
                juce::Slider::TextBoxRight, false, 48, 22);
            slider->onValueChange = [this] { submitAssistedSettings(); };
            addAndMakeVisible(*slider);
        }
        assistedMinBpm.setRange(20.0, 400.0, 1.0);
        assistedMaxBpm.setRange(20.0, 400.0, 1.0);
        assistedVariation.setRange(0.0, 100.0, 1.0);
        assistedSeed.setSelectAllWhenFocused(true);
        styleEditableTextField(assistedSeed);
        addAndMakeVisible(assistedSeed);
        configureButton(assistedApplySeed, "APPLY SEED", [this]
        {
            std::uint32_t seed = 0;
            if (!navalha::AssistedRng::parseSeed(
                    assistedSeed.getText().toStdString(), seed))
            {
                showStatus("INVALID ASSISTED SEED");
                return;
            }
            uiAssistedSeed = seed;
            submitOrWarn({
                navalha::EngineCommandType::setAssistedSeed,
                0, 0, static_cast<double>(seed)});
            assistedSeed.setText(
                navalha::AssistedRng::formatSeed(seed), false);
        });
        configureButton(assistedRewind, "REWIND", [this]
        {
            submitOrWarn({
                navalha::EngineCommandType::setAssistedSeed,
                0, 0, static_cast<double>(uiAssistedSeed)});
        });
        syncAssistedControls();

        configureParameterLabel(traceLabel, "XY MOD");
        configureButton(traceRecord, "RECORD TRACE", [this]
        {
            if (!traceArmed && !traceRecording)
            {
                stopTraceLoopFromGesture();
                traceArmed = true;
                showStatus("TRACE ARMED | DRAW ON XY MOD");
            }
            else
            {
                if (traceRecording)
                    captureTracePoint(true);
                traceArmed = false;
                traceRecording = false;
                showStatus("TRACE CANCELLED");
            }
            syncTraceControls();
        });
        configureButton(traceLoop, "TRACE LOOP", [this]
        {
            if (traceLooping)
            {
                traceLooping = false;
                submitOrWarn({navalha::EngineCommandType::stopTraceLoop});
            }
            else if (uiControlTrace.size() >= 2)
            {
                traceArmed = false;
                traceRecording = false;
                traceLooping = true;
                submitOrWarn({
                    navalha::EngineCommandType::startTraceLoop, 1});
            }
            else
                showStatus("TRACE NEEDS AT LEAST 2 POINTS");
            syncTraceControls();
        });
        configureButton(traceClear, "CLEAR", [this]
        {
            traceArmed = false;
            traceRecording = false;
            traceLooping = false;
            uiControlTrace.clear();
            submitOrWarn({navalha::EngineCommandType::clearControlTrace});
            syncTraceControls();
        });
        traceInfo.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(traceInfo);
        tracePad.onGestureStart = [this] { beginTracePadGesture(); };
        tracePad.onMove = [this] (double bpm, int pitch)
        {
            moveTracePad(bpm, pitch);
        };
        tracePad.onGestureEnd = [this] { endTracePadGesture(); };
        tracePad.setValues(tempo.getValue(), static_cast<int>(
            std::lround(pitchSemitones.getValue())));
        addAndMakeVisible(tracePad);
        syncTraceControls();

        formTransition.addItemList(
            {"CUT", "CROSSFADE", "DISSOLVE", "ACCUMULATE",
             "ERODE", "RUPTURE", "SILENCE"}, 1);
        formBankA.addItemList(
            {"WORKING", "LONG", "MEDIUM", "SHORT", "MICRO", "MANUAL", "REGION"}, 1);
        formBankB.addItemList(
            {"WORKING", "LONG", "MEDIUM", "SHORT", "MICRO", "MANUAL", "REGION"}, 1);
        for (auto* combo : {&formTransition, &formBankA, &formBankB})
        {
            combo->onChange = [this] { editFormProfiles(); };
            addAndMakeVisible(*combo);
        }
        for (auto* slider : {
                 &formDensity, &formTension, &formStability,
                 &formContinuity, &formContrast, &formStereoMotion})
        {
            slider->setRange(0.0, 100.0, 1.0);
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(
                juce::Slider::TextBoxRight, false, 44, 22);
            addAndMakeVisible(*slider);
        }
        formDensity.onValueChange = [this] { editFormProfiles(); };
        formTension.onValueChange = [this] { editFormProfiles(); };
        formStability.onValueChange = [this] { editFormProfiles(); };
        formContinuity.onValueChange = [this] { editFormCharacter(); };
        formContrast.onValueChange = [this] { editFormCharacter(); };
        formStereoMotion.onValueChange = [this] { editFormCharacter(); };
        configureButton(formLock, "LOCK", [this]
        {
            static_cast<void>(uiFormDirector.toggleCurrentLock());
            submitOrWarn({navalha::EngineCommandType::toggleFormSceneLock});
            syncFormControls();
        });
        configureButton(formAdd, "ADD", [this]
        {
            static_cast<void>(uiFormDirector.addScene());
            submitOrWarn({navalha::EngineCommandType::addFormScene});
            syncFormControls();
        });
        configureButton(formDuplicate, "COPY", [this]
        {
            static_cast<void>(uiFormDirector.duplicateScene());
            submitOrWarn({navalha::EngineCommandType::duplicateFormScene});
            syncFormControls();
        });
        configureButton(formDelete, "DELETE", [this]
        {
            static_cast<void>(uiFormDirector.deleteScene());
            submitOrWarn({navalha::EngineCommandType::deleteFormScene});
            syncFormControls();
        });
        configureButton(formMoveUp, "<", [this] { moveFormScene(-1); });
        configureButton(formMoveDown, ">", [this] { moveFormScene(1); });
        syncFormControls();

        configureParameterLabel(mixerHeaderLabel, "SOURCE MIXER");
        configureParameterLabel(mixerLevelLabel, "LEVEL");
        mixerLevelLabel.setJustificationType(juce::Justification::centred);
        mixerLevelLabel.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::ink));
        configureParameterLabel(mixerPanLabel, "PAN");
        configureParameterLabel(mixerWidthLabel, "WIDTH");
        mixerLevelLabel.setJustificationType(juce::Justification::centred);
        mixerPanLabel.setJustificationType(juce::Justification::centred);
        mixerWidthLabel.setJustificationType(juce::Justification::centred);
        for (std::size_t source = 0; source < mixerLevels.size(); ++source)
        {
            configureParameterLabel(
                mixerSourceLabels[source],
                source == 0 ? "SOURCE A" : "SOURCE B");
            mixerSourceLabels[source].setJustificationType(
                juce::Justification::centred);
            const auto sourceColour = source == 0
                ? juce::Colour(Arcade::yellowHigh)
                : juce::Colour(Arcade::red);
            mixerSourceLabels[source].setColour(
                juce::Label::textColourId, sourceColour);
            auto& level = mixerLevels[source];
            level.setName(source == 0 ? "Source A level" : "Source B level");
            level.setRange(0.0, 1.25, 0.01);
            level.setSliderStyle(juce::Slider::LinearHorizontal);
            level.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 24);
            level.setColour(juce::Slider::thumbColourId, sourceColour);
            level.onValueChange = [this, source] { submitMixer(source); };
            addAndMakeVisible(level);

            auto& pan = mixerPans[source];
            pan.setName(source == 0 ? "Source A pan" : "Source B pan");
            pan.setRange(-1.0, 1.0, 0.01);
            pan.setSliderStyle(juce::Slider::LinearHorizontal);
            pan.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 24);
            pan.setColour(juce::Slider::thumbColourId, sourceColour);
            pan.onValueChange = [this, source] { submitMixer(source); };
            addAndMakeVisible(pan);

            auto& width = mixerWidths[source];
            width.setName(source == 0 ? "Source A width" : "Source B width");
            width.setRange(0.0, 2.0, 0.01);
            width.setSliderStyle(juce::Slider::LinearHorizontal);
            width.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 24);
            width.setColour(juce::Slider::thumbColourId, sourceColour);
            width.onValueChange = [this, source] { submitMixer(source); };
            addAndMakeVisible(width);

            mixerMutes[source].setButtonText("MUTE");
            mixerMutes[source].getProperties().set(
                "arcadeAccent", source == 0 ? "sourceA" : "sourceB");
            mixerMutes[source].onClick = [this, source] { submitMixer(source); };
            addAndMakeVisible(mixerMutes[source]);
            mixerSolos[source].setButtonText("SOLO");
            mixerSolos[source].getProperties().set(
                "arcadeAccent", source == 0 ? "sourceA" : "sourceB");
            mixerSolos[source].onClick = [this, source] { submitMixer(source); };
            addAndMakeVisible(mixerSolos[source]);
        }
        syncMixerControls();
        configureParameterLabel(mixerBalanceLabel, "A/B BALANCE");
        mixerBalanceLabel.setJustificationType(juce::Justification::centred);
        mixerBalance.setRange(-1.0, 1.0, 0.01);
        mixerBalance.onValueChange = [this]
        {
            uiMixer.balance = mixerBalance.getValue();
            submitOrWarn({
                navalha::EngineCommandType::setMixerBalance,
                0,
                0,
                uiMixer.balance});
        };
        addAndMakeVisible(mixerBalance);
        mixerBalance.setValue(uiMixer.balance, juce::dontSendNotification);

        configureParameterLabel(sliceEditorLabel, "SLICES");
        configureParameterLabel(waveEditLabel, "WAVE EDIT");
        configureParameterLabel(divideRegionLabel, "DIVIDE REGION");
        sliceSource.addItemList({"SOURCE A", "SOURCE B"}, 1);
        sliceSource.setSelectedItemIndex(
            static_cast<int>(session.activeSource), juce::dontSendNotification);
        sliceSource.onChange = [this]
        {
            const auto source = selectedSliceSource();
            submitOrWarn({navalha::EngineCommandType::selectSource, source});
            refreshSliceEditor();
            showSelectedSourceWaveform();
        };
        addAndMakeVisible(sliceSource);

        sliceIndex.onChange = [this] { refreshSliceBounds(); };
        addAndMakeVisible(sliceIndex);
        for (auto* slider : {&sliceStart, &sliceEnd})
        {
            slider->setRange(0.0, 1.0, 0.001);
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 24);
            addAndMakeVisible(*slider);
        }
        sliceStart.setName("Slice start");
        sliceEnd.setName("Slice end");
        configureButton(setSlice, "SET", [this] { applySliceBounds(); });
        configureButton(selectRegionMode, "SELECT REGION", [this]
        {
            setWaveformEditMode(WaveformComponent::EditMode::region);
        });
        configureButton(editSliceMode, "EDIT SLICE", [this]
        {
            setWaveformEditMode(WaveformComponent::EditMode::slice);
        });
        configureButton(bladeMode, "BLADE", [this]
        {
            setWaveformEditMode(WaveformComponent::EditMode::blade);
        });
        configureButton(undoBlade, "UNDO", [this] { undoBladeCut(); });
        configureButton(wholeRegion, "WHOLE", [this] { selectWholeRegion(); });
        constexpr std::array<std::size_t, 5> regionDivisions {4, 8, 16, 32, 64};
        for (std::size_t index = 0; index < divideRegionButtons.size(); ++index)
        {
            const auto count = regionDivisions[index];
            configureButton(
                divideRegionButtons[index], juce::String(count),
                [this, count] { divideSelectedRegion(count); });
        }
        configureButton(playSlice, "PLAY SLICE", [this]
        {
            submitOrWarn({
                navalha::EngineCommandType::triggerSlice,
                selectedSliceSource(), selectedSliceIndex()});
            showStatus("SLICE AUDITION");
        });
        setWaveformEditMode(WaveformComponent::EditMode::region);
        refreshSliceEditor();

        configureParameterLabel(
            voicesHeaderLabel,
            "VIRTUAL VOICES        ENABLE       SOURCE       DIVISION"
            "                 PITCH                         LEVEL"
            "                          PAN");
        for (std::size_t voice = 0; voice < voiceEnabled.size(); ++voice)
        {
            configureParameterLabel(
                voiceLabels[voice], voice == 0 ? "VOICE 1" : "VOICE 2");
            voiceEnabled[voice].setButtonText("ON");
            voiceEnabled[voice].onClick = [this, voice]
            {
                submitVoiceProperty(
                    voice, navalha::VirtualVoiceProperty::enabled,
                    voiceEnabled[voice].getToggleState() ? 1.0 : 0.0);
            };
            addAndMakeVisible(voiceEnabled[voice]);

            voiceSources[voice].addItemList({"A", "B"}, 1);
            voiceSources[voice].onChange = [this, voice]
            {
                submitVoiceProperty(
                    voice, navalha::VirtualVoiceProperty::source,
                    static_cast<double>(
                        std::max(0, voiceSources[voice].getSelectedItemIndex())));
            };
            addAndMakeVisible(voiceSources[voice]);

            voiceDivisions[voice].addItemList({"1", "2", "4", "8"}, 1);
            voiceDivisions[voice].onChange = [this, voice]
            {
                const auto index = static_cast<std::size_t>(
                    std::max(0, voiceDivisions[voice].getSelectedItemIndex()));
                submitVoiceProperty(
                    voice, navalha::VirtualVoiceProperty::division,
                    static_cast<double>(
                        std::array<std::size_t, 4> {1, 2, 4, 8}[index]));
            };
            addAndMakeVisible(voiceDivisions[voice]);

            voicePitches[voice].setRange(-12.0, 11.0, 1.0);
            voicePitches[voice].setTextValueSuffix(" st");
            voicePitches[voice].onValueChange = [this, voice]
            {
                submitVoiceProperty(
                    voice, navalha::VirtualVoiceProperty::pitch,
                    voicePitches[voice].getValue());
            };
            addAndMakeVisible(voicePitches[voice]);

            voiceLevels[voice].setRange(0.0, 0.8, 0.01);
            voiceLevels[voice].onValueChange = [this, voice]
            {
                submitVoiceProperty(
                    voice, navalha::VirtualVoiceProperty::level,
                    voiceLevels[voice].getValue());
            };
            addAndMakeVisible(voiceLevels[voice]);

            voicePans[voice].setRange(-1.0, 1.0, 0.01);
            voicePans[voice].onValueChange = [this, voice]
            {
                submitVoiceProperty(
                    voice, navalha::VirtualVoiceProperty::pan,
                    voicePans[voice].getValue());
            };
            addAndMakeVisible(voicePans[voice]);
        }
        syncVoiceControls();

        configureParameterLabel(voiceAdvancedLabel, "VOICE DETAIL");
        voiceEditor.addItemList({"VOICE 1", "VOICE 2"}, 1);
        voiceEditor.setSelectedItemIndex(0, juce::dontSendNotification);
        voiceEditor.onChange = [this] { refreshAdvancedVoiceControls(); };
        addAndMakeVisible(voiceEditor);

        voicePatternLength.setRange(1.0, 16.0, 1.0);
        voicePatternLength.setName("Virtual voice pattern length");
        voicePatternLength.onValueChange = [this]
        {
            submitVoiceProperty(
                selectedVoice(), navalha::VirtualVoiceProperty::patternLength,
                voicePatternLength.getValue());
            refreshVirtualPatternCells();
        };
        addAndMakeVisible(voicePatternLength);

        for (auto* slider : {
                 &voiceFocusStart, &voiceFocusEnd, &voiceAttack, &voiceRelease})
        {
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 24);
            addAndMakeVisible(*slider);
        }
        voiceFocusStart.setRange(0.0, 1.0, 0.01);
        voiceFocusEnd.setRange(0.0, 1.0, 0.01);
        voiceAttack.setRange(0.001, 0.5, 0.001);
        voiceRelease.setRange(0.005, 1.5, 0.005);
        voiceFocusStart.setName("Virtual voice focus start");
        voiceFocusEnd.setName("Virtual voice focus end");
        voiceAttack.setName("Virtual voice attack seconds");
        voiceRelease.setName("Virtual voice release seconds");
        voiceFocusStart.onValueChange = [this]
        {
            submitVoiceProperty(
                selectedVoice(), navalha::VirtualVoiceProperty::focusStart,
                voiceFocusStart.getValue());
        };
        voiceFocusEnd.onValueChange = [this]
        {
            submitVoiceProperty(
                selectedVoice(), navalha::VirtualVoiceProperty::focusEnd,
                voiceFocusEnd.getValue());
        };
        voiceAttack.onValueChange = [this]
        {
            submitVoiceProperty(
                selectedVoice(), navalha::VirtualVoiceProperty::attack,
                voiceAttack.getValue());
        };
        voiceRelease.onValueChange = [this]
        {
            submitVoiceProperty(
                selectedVoice(), navalha::VirtualVoiceProperty::release,
                voiceRelease.getValue());
        };

        configureParameterLabel(voicePatternLabel, "VOICE PATTERN");
        for (std::size_t step = 0; step < voicePatternCells.size(); ++step)
        {
            auto& cell = voicePatternCells[step];
            cell.setRange(0.0, 127.0, 1.0);
            cell.setSliderStyle(juce::Slider::LinearBar);
            cell.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 22);
            cell.onValueChange = [this, step]
            {
                const auto voice = selectedVoice();
                const auto value = static_cast<std::uint8_t>(
                    std::lround(voicePatternCells[step].getValue()));
                uiVirtualVoices[voice].pattern[step] = value;
                submitOrWarn({
                    navalha::EngineCommandType::setVirtualVoicePatternCell,
                    voice,
                    step,
                    static_cast<double>(value)});
            };
            addAndMakeVisible(cell);
        }
        refreshAdvancedVoiceControls();

        configureParameterLabel(outputMeterLabel, "MASTER OUT");
        addAndMakeVisible(outputLeftMeter);
        addAndMakeVisible(outputRightMeter);
        configureParameterLabel(recordingFormatLabel, "REC FORMAT");
        recordingFormat.addItemList({"PCM16", "PCM24", "FLOAT32"}, 1);
        recordingFormat.setSelectedItemIndex(1, juce::dontSendNotification);
        addAndMakeVisible(recordingFormat);
        recordingInfo.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(recordingInfo);

        configureButton(audioConnectionStatus, "AUDIO CONNECTING...", [this]
        {
            showAudioSetup();
        });
        audioConnectionStatus.setColour(
            juce::TextButton::buttonColourId,
            juce::Colour(Arcade::surfaceHigh));
        status.setText("READY", juce::dontSendNotification);
        status.setJustificationType(juce::Justification::centredLeft);
        status.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        addAndMakeVisible(status);
        configureParameterLabel(libraryLabel, "AUDIO LIBRARY");
        libraryLabel.setJustificationType(juce::Justification::centredLeft);
        libraryLabel.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 13.0F, juce::Font::bold)));
        libraryLabel.getProperties().set("arcadeFontSize", 13.0);
        libraryLabel.getProperties().set("arcadeFontBold", true);
        libraryLabel.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        libraryLabel.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surfaceHigh));
        libraryLabel.setColour(
            juce::Label::outlineColourId, juce::Colour(Arcade::line));
        auto libraryRoot = juce::File::getSpecialLocation(
            juce::File::userMusicDirectory);
        if (!libraryRoot.isDirectory())
            libraryRoot = juce::File::getCurrentWorkingDirectory();
        previewFormatManager.registerBasicFormats();
        audioLibrary.setRootDirectory(libraryRoot);
        audioLibrary.onSelection = [this] (const juce::File& file)
        {
            selectedLibraryFile = file;
            const auto sizeInMb =
                static_cast<double>(file.getSize()) / (1024.0 * 1024.0);
            selectedInfo.setText(
                file.getFileName() + "\n"
                    + file.getFileExtension().trimCharactersAtStart(".").toUpperCase()
                    + " | " + juce::String(sizeInMb, 1)
                    + juce::String::fromUTF8(" MB · READY TO LOAD"),
                juce::dontSendNotification);
            selectedInfo.setTooltip(file.getFullPathName());
        };
        audioLibrary.onPreview = [this] (const juce::File& file)
        {
            startAudioPreview(file);
        };
        addAndMakeVisible(audioLibrary);
        configureButton(loadSelectedA, "LOAD A", [this]
        {
            if (selectedLibraryFile.existsAsFile())
                loadSource(0, selectedLibraryFile);
            else
                showStatus("SELECT AN AUDIO FILE FIRST");
        });
        configureButton(loadSelectedB, "LOAD B", [this]
        {
            if (selectedLibraryFile.existsAsFile())
                loadSource(1, selectedLibraryFile);
            else
                showStatus("SELECT AN AUDIO FILE FIRST");
        });
        configureButton(previewSelected, "PREVIEW", [this]
        {
            if (selectedLibraryFile.existsAsFile())
                startAudioPreview(selectedLibraryFile);
            else
                showStatus("SELECT AN AUDIO FILE FIRST");
        });
        configureButton(stopPreview, "STOP", [this] { stopAudioPreview(); });
        configureButton(
            chooseLibraryFolder, "CHOOSE FOLDER...",
            [this] { chooseAudioLibraryDirectory(); });
        libraryPath.setText(
            libraryRoot.getFileName().isEmpty()
                ? libraryRoot.getFullPathName()
                : libraryRoot.getFileName(),
            juce::dontSendNotification);
        libraryPath.setTooltip(libraryRoot.getFullPathName());
        libraryPath.setJustificationType(juce::Justification::centredLeft);
        libraryPath.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surface));
        libraryPath.setColour(
            juce::Label::outlineColourId, juce::Colour(Arcade::line));
        libraryPath.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::ink));
        libraryPath.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 10.0F, juce::Font::plain)));
        libraryPath.getProperties().set("arcadeFontSize", 10.0);
        addAndMakeVisible(libraryPath);
        librarySearch.setTextToShowWhenEmpty(
            "SEARCH FILES...", juce::Colour(Arcade::muted));
        styleEditableTextField(librarySearch);
        librarySearch.onTextChange = [this]
        {
            audioLibrary.setFilterText(librarySearch.getText());
            refreshLibraryHint();
        };
        addAndMakeVisible(librarySearch);
        libraryHint.setJustificationType(juce::Justification::centredLeft);
        libraryHint.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        libraryHint.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surface));
        addAndMakeVisible(libraryHint);
        refreshLibraryHint();
        configureParameterLabel(logLabel, "ACTIVITY LOG");
        logLabel.setJustificationType(juce::Justification::centredLeft);
        logLabel.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 12.0F, juce::Font::bold)));
        logLabel.getProperties().set("arcadeFontSize", 12.0);
        logLabel.getProperties().set("arcadeFontBold", true);
        logLabel.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        logLabel.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surfaceHigh));
        logLabel.setColour(
            juce::Label::outlineColourId, juce::Colour(Arcade::line));
        configureButton(copyLog, "COPY", [this]
        {
            juce::SystemClipboard::copyTextToClipboard(activityLog.getText());
            showStatus("ACTIVITY LOG COPIED");
        });
        configureButton(clearLog, "CLEAR", [this]
        {
            activityLog.clear();
        });
        configureParameterLabel(selectedLabel, "SELECTED FILE");
        selectedInfo.setText("No file selected", juce::dontSendNotification);
        selectedInfo.setJustificationType(juce::Justification::topLeft);
        selectedInfo.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 10.0F, juce::Font::plain)));
        selectedInfo.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        selectedInfo.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surface));
        addAndMakeVisible(selectedInfo);
        activityLog.setMultiLine(true);
        activityLog.setReadOnly(true);
        activityLog.setScrollbarsShown(false);
        activityLog.setCaretVisible(false);
        activityLog.setText("AUDIO ENGINE READY", false);
        activityLog.setColour(
            juce::TextEditor::backgroundColourId, juce::Colour(Arcade::surface));
        activityLog.setColour(
            juce::TextEditor::outlineColourId, juce::Colour(Arcade::line));
        activityLog.setColour(
            juce::TextEditor::textColourId, juce::Colour(Arcade::muted));
        activityLog.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 11.0F, juce::Font::plain)));
        addAndMakeVisible(activityLog);
        learnModeLabel.setText("LEARNING MODE", juce::dontSendNotification);
        learnModeLabel.setJustificationType(juce::Justification::centredLeft);
        learnModeLabel.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 10.5F, juce::Font::bold)));
        learnModeLabel.getProperties().set("arcadeFontSize", 10.5);
        learnModeLabel.getProperties().set("arcadeFontBold", true);
        learnModeLabel.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        learnModeLabel.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surfaceHigh));
        addChildComponent(learnModeLabel);
        learnTitle.setJustificationType(juce::Justification::centredLeft);
        learnTitle.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 11.0F, juce::Font::bold)));
        learnTitle.getProperties().set("arcadeFontSize", 11.0);
        learnTitle.getProperties().set("arcadeFontBold", true);
        learnTitle.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::ink));
        learnTitle.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surface));
        addChildComponent(learnTitle);
        learnBody.setMultiLine(true);
        learnBody.setReadOnly(true);
        learnBody.setScrollbarsShown(false);
        learnBody.setCaretVisible(false);
        learnBody.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 10.5F, juce::Font::plain)));
        learnBody.setColour(
            juce::TextEditor::backgroundColourId, juce::Colour(Arcade::surface));
        learnBody.setColour(
            juce::TextEditor::outlineColourId, juce::Colour(Arcade::line));
        learnBody.setColour(
            juce::TextEditor::textColourId, juce::Colour(Arcade::muted));
        addChildComponent(learnBody);
        waveform.onFileDropped =
            [this] (std::size_t sourceIndex, const juce::File& file)
            {
                loadSource(sourceIndex, file);
            };
        waveform.onSourceSelected = [this] (std::size_t sourceIndex)
        {
            sliceSource.setSelectedItemIndex(
                static_cast<int>(sourceIndex), juce::sendNotification);
        };
        waveform.onRangeSelected =
            [this] (std::size_t sourceIndex, double start, double end)
            {
                handleWaveformRange(sourceIndex, start, end);
            };
        waveform.onBladeCut =
            [this] (std::size_t sourceIndex, double position)
            {
                addBladeCut(sourceIndex, position);
            };
        addAndMakeVisible(waveform);
        for (auto* heading : {
                 &patternCellsLabel, &sliceEditorLabel, &waveEditLabel,
                 &divideRegionLabel, &orderLabel,
                 &gestureLabel, &formLabel, &traceLabel, &assistedLabel,
                 &motifLabel, &mixerHeaderLabel, &voicesHeaderLabel,
                 &voiceAdvancedLabel,
                 &voicePatternLabel})
            promoteModuleHeading(*heading);
        promoteModuleHeading(selectedLabel, true);
        selectedLabel.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surfaceHigh));
        mixerHeaderLabel.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surfaceHigh));
        setSize(1100, 1366);
        initialiseAudioDevice();
        initialiseUiPreferences();
        configureLearningMetadata();
#if JUCE_DEBUG
        for (int index = 0; index < getNumChildComponents(); ++index)
            jassert(getChildComponent(index)->getProperties().contains(
                "learnKey"));
#endif
        addMouseListener(this, true);
        juce::Desktop::getInstance().addFocusChangeListener(this);
        deviceManager.addChangeListener(this);
        refreshAudioConnectionStatus();
        startTimerHz(30);
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(Arcade::background));
        auto header = getLocalBounds().reduced(12).removeFromTop(60).toFloat();
        juce::ColourGradient glow(
            juce::Colour(0x18ffd84a), header.getCentreX(), header.getY(),
            juce::Colours::transparentBlack, header.getCentreX(), header.getBottom(),
            false);
        graphics.setGradientFill(glow);
        graphics.fillRect(header);
        graphics.setColour(juce::Colour(Arcade::yellow));
        graphics.fillRect(header.removeFromLeft(7.0F));
        graphics.setColour(juce::Colour(Arcade::line));
        graphics.drawRect(getLocalBounds().toFloat().reduced(12.5F), 1.0F);

        const auto drawModule = [&graphics, this] (int y, int height,
                                                    const juce::String& name,
                                                    juce::Colour accent,
                                                    bool floatingLabel)
        {
            auto bounds = juce::Rectangle<float>(
                8.0F, static_cast<float>(y),
                static_cast<float>(getWidth() - 16), static_cast<float>(height));
            // Each workspace carries a restrained translucent wash of its
            // rail colour; child controls remain opaque for legibility.
            graphics.setColour(tintedPanelSurface(accent));
            graphics.fillRoundedRectangle(bounds, 4.0F);
            graphics.setColour(juce::Colour(Arcade::line));
            graphics.drawRoundedRectangle(bounds, 4.0F, 1.0F);
            auto rail = bounds.removeFromLeft(34.0F);
            graphics.setColour(accent.withAlpha(0.20F));
            graphics.fillRect(rail);
            graphics.setColour(accent);
            graphics.fillRect(
                rail.getX(), rail.getY(), 5.0F, rail.getHeight());
            graphics.drawRect(rail, 1.0F);
            if (name.isNotEmpty())
            {
                auto labelRail = rail;
                if (floatingLabel)
                {
                    auto visibleArea = getLocalBounds().toFloat();
                    if (auto* viewport =
                            findParentComponentOfClass<juce::Viewport>())
                        visibleArea = viewport->getViewArea().toFloat();
                    const auto visibleRail = rail.getIntersection(
                        visibleArea);
                    if (!visibleRail.isEmpty())
                    {
                        constexpr float labelHeight = 190.0F;
                        const auto halfHeight = labelHeight * 0.5F;
                        const auto centreY = juce::jlimit(
                            rail.getY() + halfHeight,
                            rail.getBottom() - halfHeight,
                            visibleRail.getCentreY());
                        labelRail = juce::Rectangle<float>(
                            rail.getX(), centreY - halfHeight,
                            rail.getWidth(), labelHeight);
                    }
                }
                graphics.saveState();
                graphics.addTransform(
                    juce::AffineTransform::rotation(
                        -juce::MathConstants<float>::halfPi)
                        .translated(labelRail.getX(), labelRail.getBottom()));
                graphics.setColour(accent.brighter(0.35F));
                graphics.setFont(juce::Font(juce::FontOptions(
                    "DejaVu Sans Mono", 10.0F, juce::Font::bold)));
                graphics.drawFittedText(
                    name, 4, 0,
                    static_cast<int>(labelRail.getHeight()) - 8,
                    static_cast<int>(labelRail.getWidth()),
                    juce::Justification::centred, 1);
                graphics.restoreState();
            }
        };
        const auto topBottom = dualMonitorLayout ? 168 : 208;
        drawModule(
            8, topBottom - 8, "TOP / TRANSPORT",
            juce::Colour(Arcade::steel), false);
        drawModule(
            topBottom, dualMonitorLayout ? 224 : 288, "PREPARE / WAVEFORM",
            juce::Colour(Arcade::yellow), false);
        if (dualMonitorLayout)
        {
            drawModule(
                392, juce::jmax(120, getHeight() - 396),
                "CREATE / VOICES", juce::Colour(Arcade::red), false);
        }
        else
        {
            drawModule(
                496, 640, "PERFORM / CREATE", juce::Colour(Arcade::red), true);
            drawModule(
                1136, 226, "VIRTUAL VOICES",
                juce::Colour(Arcade::yellow), false);
        }

        auto visibleArea = visibleViewportArea.isEmpty()
            ? getLocalBounds() : visibleViewportArea;
        auto contextualRail = juce::Rectangle<float>(
            static_cast<float>(getWidth() - 12 - Arcade::contextualRailWidth),
            static_cast<float>(visibleArea.getY()),
            static_cast<float>(Arcade::contextualRailWidth),
            static_cast<float>(juce::jmax(1, visibleArea.getHeight() - 4)));
        graphics.setColour(juce::Colour(Arcade::surface));
        graphics.fillRect(contextualRail);
        graphics.setColour(juce::Colour(Arcade::line));
        graphics.drawRect(contextualRail, 1.0F);

        // The approved Arcade brand is the topmost header element. Module
        // backgrounds must never cover it.
        if (arcadeLogo != nullptr)
        {
            constexpr float brandShift = 28.0F;
            const auto logoBounds = juce::Rectangle<float>(
                18.0F + brandShift, 18.0F, 300.0F, 62.0F);
            graphics.saveState();
            graphics.reduceClipRegion(
                juce::Rectangle<int>(
                    128 + static_cast<int>(brandShift), 8, 200, 80));
            arcadeLogo->drawWithin(
                graphics, logoBounds,
                juce::RectanglePlacement::centred, 1.0F);
            graphics.restoreState();

            graphics.saveState();
            graphics.reduceClipRegion(
                juce::Rectangle<int>(
                    18 + static_cast<int>(brandShift), 8, 90, 76));
            const auto mascotScale = 74.0F / 440.0F;
            const auto mascotTransform =
                juce::AffineTransform::translation(-16.0F, -20.0F)
                    .scaled(mascotScale)
                    .translated(18.0F + brandShift, 8.0F);
            arcadeLogo->draw(graphics, 1.0F, mascotTransform);
            graphics.restoreState();
        }
    }

    ~MainComponent() override
    {
        stopTimer();
        juce::Desktop::getInstance().removeFocusChangeListener(this);
        removeMouseListener(this);
        deviceManager.removeChangeListener(this);
        saveTakeCatalog();
        saveAudioSettings();
        recorder.stop();
        stopAudioPreview(false);
        engine.suspendOutput();
        shutdownAudio();
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        activeSampleRate.store(sampleRate, std::memory_order_release);
        engine.setOutputProfile(navalha::OutputProfile::liveSafe);
        engine.prepare(sampleRate);
        previewTransport.prepareToPlay(samplesPerBlockExpected, sampleRate);
        engine.resumeOutput();
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& output) override
    {
        output.clearActiveBufferRegion();

        const float* previewLeft = nullptr;
        const float* previewRight = nullptr;
        if (previewTransport.isPlaying()
            && output.numSamples <= previewScratch.getNumSamples())
        {
            previewScratch.clear();
            juce::AudioSourceChannelInfo preview(
                &previewScratch, 0, output.numSamples);
            previewTransport.getNextAudioBlock(preview);
            previewLeft = previewScratch.getReadPointer(0);
            previewRight = previewScratch.getReadPointer(1);
        }

        if (output.buffer->getNumChannels() >= 2)
        {
            engine.processBlock(
                output.buffer->getWritePointer(0, output.startSample),
                output.buffer->getWritePointer(1, output.startSample),
                static_cast<std::size_t>(output.numSamples),
                previewLeft, previewRight);
        }
    }

    void releaseResources() override
    {
        engine.suspendOutput();
        previewTransport.releaseResources();
        engine.stop();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        area.removeFromLeft(34);
        auto rightRailColumn = area.removeFromRight(
            juce::jmin(Arcade::contextualRailWidth, area.getWidth() / 4));
        area.removeFromRight(8);
        auto visibleArea = visibleViewportArea.isEmpty()
            ? getLocalBounds() : visibleViewportArea;
        auto rightRail = juce::Rectangle<int>(
            rightRailColumn.getX(), visibleArea.getY(),
            rightRailColumn.getWidth(),
            juce::jmax(120, visibleArea.getHeight() - 4));
        area.removeFromTop(10);
        auto transportPanel = rightRail.removeFromTop(76);
        transportClock.setBounds(
            transportPanel.removeFromTop(34).reduced(2, 1));
        auto transportButtons = transportPanel;
        const auto railButtonWidth = transportButtons.getWidth() / 4;
        stop.setBounds(
            transportButtons.removeFromLeft(railButtonWidth).reduced(2));
        play.setBounds(
            transportButtons.removeFromLeft(railButtonWidth).reduced(2));
        record.setBounds(
            transportButtons.removeFromLeft(railButtonWidth).reduced(2));
        resetTransport.setBounds(transportButtons.reduced(2));
        rightRail.removeFromTop(8);

        const auto placeEngineStatusStrip = [this] (
            juce::Rectangle<int> strip)
        {
            audioConnectionStatus.setBounds(
                strip.removeFromLeft(145).reduced(2, 4));
            status.setBounds(
                strip.removeFromLeft(90).reduced(2, 4));
            outputMeterLabel.setBounds(
                strip.removeFromLeft(75).reduced(2, 4));
            outputLeftMeter.setBounds(
                strip.removeFromLeft(78).reduced(2, 4));
            outputRightMeter.setBounds(
                strip.removeFromLeft(78).reduced(2, 4));
            recordingFormatLabel.setBounds(
                strip.removeFromLeft(78).reduced(2, 4));
            recordingFormat.setBounds(
                strip.removeFromLeft(86).reduced(2, 4));
            recordingInfo.setBounds(strip.reduced(2, 4));
        };

        auto headerRow = area.removeFromTop(54);
        title.setBounds(headerRow.removeFromLeft(320));
        auto controls = headerRow.reduced(0, 5);
        auto projectModule = controls.removeFromLeft(
            juce::jmin(
                dualMonitorLayout ? 330 : 300,
                controls.getWidth() / 2));
        const auto headerButtonWidth = projectModule.getWidth() / 3;
        openProject.setBounds(
            projectModule.removeFromLeft(headerButtonWidth).reduced(4));
        saveProject.setBounds(
            projectModule.removeFromLeft(headerButtonWidth).reduced(4));
        savePortable.setBounds(projectModule.reduced(4));
        juce::Rectangle<int> heritageHeader;
        if (!dualMonitorLayout)
        {
            heritageHeader = controls.removeFromRight(
                juce::jmin(280,
                           juce::jmax(250, controls.getWidth() / 2)));
            pitchBypass.setBounds(
                heritageHeader.removeFromLeft(
                    heritageHeader.getWidth() * 3 / 5).reduced(4));
            pitchAudition.setBounds(heritageHeader.reduced(4));
        }
        if (dualMonitorLayout)
        {
            if (controls.getWidth() >= 760)
            {
                // Wide dual-monitor header: one consistent-height strip uses
                // the full width instead of leaving large status-label gaps.
                placeEngineStatusStrip(controls);
            }
            else
            {
                // Compact fallback for mixed-resolution dual-monitor setups.
                auto engineStatusRow = controls.removeFromTop(
                    controls.getHeight() / 2);
                audioConnectionStatus.setBounds(
                    engineStatusRow.removeFromLeft(
                        juce::jmin(150, engineStatusRow.getWidth() / 2))
                        .reduced(2, 1));
                status.setBounds(engineStatusRow.reduced(2, 1));
                auto meterRow = controls;
                const auto cellWidth = juce::jmax(
                    48, meterRow.getWidth() / 6);
                outputMeterLabel.setBounds(
                    meterRow.removeFromLeft(cellWidth).reduced(2, 1));
                outputLeftMeter.setBounds(
                    meterRow.removeFromLeft(cellWidth).reduced(2, 1));
                outputRightMeter.setBounds(
                    meterRow.removeFromLeft(cellWidth).reduced(2, 1));
                recordingFormatLabel.setBounds(
                    meterRow.removeFromLeft(cellWidth).reduced(2, 1));
                recordingFormat.setBounds(
                    meterRow.removeFromLeft(cellWidth).reduced(2, 1));
                recordingInfo.setBounds(meterRow.reduced(2, 1));
            }
        }
        auto sequencerControls = area.removeFromTop(44);
        auto heritageControls = sequencerControls.removeFromRight(
            dualMonitorLayout ? 320 : 90);
        heritageControls.removeFromLeft(10);
        if (dualMonitorLayout)
        {
            constexpr int fixedLabelsAndApply =
                38 + 38 + 48 + 45 + 48 + 60 + 52 + 38 + 45;
            const auto valueWidth = juce::jmax(
                74,
                (sequencerControls.getWidth() - fixedLabelsAndApply) / 8);
            const auto place = [&sequencerControls, valueWidth]
                (juce::Label& label, int labelWidth, juce::Component& value)
            {
                label.setBounds(
                    sequencerControls.removeFromLeft(labelWidth).reduced(2));
                value.setBounds(
                    sequencerControls.removeFromLeft(valueWidth).reduced(2));
            };
            place(tempoLabel, 38, tempo);
            place(divisionLabel, 38, division);
            place(patternLabel, 48, pattern);
            place(timingLabel, 45, timing);
            place(jitterLabel, 48, jitterControl);
            timingSeedLabel.setBounds(
                sequencerControls.removeFromLeft(60).reduced(2));
            timingSeedEditor.setBounds(
                sequencerControls.removeFromLeft(valueWidth).reduced(2));
            applyTimingSeed.setBounds(
                sequencerControls.removeFromLeft(52).reduced(2));
            place(pitchLabel, 38, pitchSemitones);
            pitchMixLabel.setBounds(
                sequencerControls.removeFromLeft(45).reduced(2));
            pitchMix.setBounds(sequencerControls.reduced(2));
            transportInfo.setBounds(0, 0, 0, 0);
        }
        else
        {
            constexpr int sequencerLabelWidth = 320;
            const auto sequencerValueWidth = juce::jmax(
                70, (sequencerControls.getWidth() - sequencerLabelWidth) / 6);
            tempoLabel.setBounds(
                sequencerControls.removeFromLeft(45).reduced(3));
            tempo.setBounds(sequencerControls.removeFromLeft(
                sequencerValueWidth).reduced(3));
            divisionLabel.setBounds(
                sequencerControls.removeFromLeft(45).reduced(3));
            division.setBounds(sequencerControls.removeFromLeft(
                sequencerValueWidth).reduced(3));
            patternLabel.setBounds(
                sequencerControls.removeFromLeft(60).reduced(3));
            pattern.setBounds(sequencerControls.removeFromLeft(
                sequencerValueWidth).reduced(3));
            timingLabel.setBounds(
                sequencerControls.removeFromLeft(55).reduced(3));
            timing.setBounds(sequencerControls.removeFromLeft(
                sequencerValueWidth).reduced(3));
            pitchLabel.setBounds(
                sequencerControls.removeFromLeft(45).reduced(3));
            pitchSemitones.setBounds(sequencerControls.removeFromLeft(
                sequencerValueWidth).reduced(3));
            pitchMixLabel.setBounds(
                sequencerControls.removeFromLeft(70).reduced(3));
            pitchMix.setBounds(sequencerControls.removeFromLeft(
                sequencerValueWidth).reduced(3));
        }
        if (dualMonitorLayout)
        {
            pitchBypass.setBounds(
                heritageControls.removeFromLeft(130).reduced(3));
            pitchZero.setBounds(
                heritageControls.removeFromLeft(52).reduced(3));
            pitchAudition.setBounds(heritageControls.reduced(3));
        }
        else
        {
            pitchBypass.setBounds(
                heritageHeader.removeFromLeft(
                    heritageHeader.getWidth() * 3 / 5).reduced(4));
            pitchAudition.setBounds(heritageHeader.reduced(4));
            pitchZero.setBounds(heritageControls.reduced(3));

            // Single-monitor fallback: reserve a deterministic header strip
            // so these two controls cannot collapse when the surrounding
            // project/status blocks become narrow.
            const auto headerStrip = juce::Rectangle<int>(
                headerRow.getRight() - 280, headerRow.getY() + 5, 280, 44);
            auto largeHeritage = headerStrip;
            pitchBypass.setBounds(largeHeritage.removeFromLeft(168).reduced(4));
            pitchAudition.setBounds(largeHeritage.reduced(4));
        }
        if (!dualMonitorLayout)
        {
            auto timingControls = area.removeFromTop(40);
            jitterLabel.setBounds(timingControls.removeFromLeft(85).reduced(3));
            jitterControl.setBounds(timingControls.removeFromLeft(260).reduced(3));
            timingSeedLabel.setBounds(
                timingControls.removeFromLeft(115).reduced(3));
            timingSeedEditor.setBounds(
                timingControls.removeFromLeft(220).reduced(3));
            applyTimingSeed.setBounds(
                timingControls.removeFromLeft(80).reduced(3));
            transportInfo.setBounds(timingControls.reduced(3));
        }
        auto patternControls = area.removeFromTop(48);
        patternCellsLabel.setBounds(patternControls.removeFromLeft(70).reduced(4));
        const auto cellWidth = patternControls.getWidth() / 8;
        for (auto& cell : patternCells)
            cell.setBounds(patternControls.removeFromLeft(cellWidth).reduced(3));

        // Continue the fixed black rail below transport with library, log/LEARN
        // and SOURCE MIXER. The production workspace never scrolls over it.
        constexpr int mixerRailHeight = 364;
        auto libraryRail = rightRail.removeFromTop(
            juce::jmax(280, rightRail.getHeight() - mixerRailHeight - 8));
        rightRail.removeFromTop(8);
        auto mixerRail = rightRail;

        libraryLabel.setBounds(libraryRail.removeFromTop(24));
        auto libraryFolderRow = libraryRail.removeFromTop(34);
        chooseLibraryFolder.setBounds(
            libraryFolderRow.removeFromLeft(132).reduced(0, 2));
        libraryPath.setBounds(libraryFolderRow.reduced(4, 2));
        librarySearch.setBounds(libraryRail.removeFromTop(28).reduced(0, 2));
        libraryHint.setBounds(libraryRail.removeFromTop(18).reduced(4, 0));
        auto logPanel = libraryRail.removeFromBottom(
            learningMode
                ? (dualMonitorLayout ? 164 : 190)
                : (dualMonitorLayout ? 120 : 170));
        auto logHeader = logPanel.removeFromTop(26);
        logLabel.setBounds(logHeader.removeFromLeft(
            juce::jmax(80, logHeader.getWidth() - 116)));
        copyLog.setBounds(logHeader.removeFromLeft(56).reduced(2));
        clearLog.setBounds(logHeader.reduced(2));
        if (learningMode)
        {
            // Three text lines are the safe minimum for detailed help and
            // translations; the default two-line message must never clip.
            auto learnPanel = logPanel.removeFromBottom(82);
            learnModeLabel.setBounds(learnPanel.removeFromTop(18));
            learnTitle.setBounds(learnPanel.removeFromTop(20).reduced(4, 0));
            learnBody.setBounds(learnPanel.reduced(2));
        }
        activityLog.setBounds(logPanel);
        auto selectedPanel = libraryRail.removeFromBottom(
            dualMonitorLayout ? 94 : 106);
        selectedLabel.setBounds(selectedPanel.removeFromTop(22));
        selectedInfo.setBounds(selectedPanel.removeFromTop(38).reduced(4, 1));
        auto selectedActions = selectedPanel;
        const auto selectedActionWidth = selectedActions.getWidth() / 4;
        previewSelected.setBounds(
            selectedActions.removeFromLeft(selectedActionWidth).reduced(2));
        stopPreview.setBounds(
            selectedActions.removeFromLeft(selectedActionWidth).reduced(2));
        loadSelectedA.setBounds(
            selectedActions.removeFromLeft(selectedActionWidth).reduced(2));
        loadSelectedB.setBounds(selectedActions.reduced(2));
        audioLibrary.setBounds(libraryRail.withTrimmedBottom(8));

        // Preserve the original Navalha hierarchy: PREPARE and its waveform
        // remain visible before the denser performance/director controls.
        area.removeFromTop(18);
        auto prepareArea = area.removeFromTop(dualMonitorLayout ? 206 : 270);
        if (!dualMonitorLayout)
        {
            // Single-monitor mode keeps the complete engine strip in
            // PREPARE / WAVEFORM, but uses the same one-line order as dual.
            placeEngineStatusStrip(prepareArea.removeFromTop(44));
        }
        // Output/recording status belongs above the waveform. The interaction
        // strip immediately below the waveform is reserved for region and
        // slice editing, matching the direct-manipulation PD workflow.
        auto waveEditRow = prepareArea.removeFromBottom(44);
        auto prepareContent = prepareArea.reduced(4);
        waveform.setBounds(prepareContent);
        waveEditLabel.setBounds(waveEditRow.removeFromLeft(84).reduced(2, 3));
        selectRegionMode.setBounds(
            waveEditRow.removeFromLeft(122).reduced(2, 3));
        editSliceMode.setBounds(
            waveEditRow.removeFromLeft(100).reduced(2, 3));
        bladeMode.setBounds(waveEditRow.removeFromLeft(72).reduced(2, 3));
        wholeRegion.setBounds(waveEditRow.removeFromLeft(70).reduced(2, 3));
        undoBlade.setBounds(waveEditRow.removeFromLeft(70).reduced(2, 3));
        divideRegionLabel.setBounds(
            waveEditRow.removeFromLeft(116).reduced(2, 3));
        const auto divisionButtonWidth = juce::jmax(
            38, waveEditRow.getWidth()
                / static_cast<int>(divideRegionButtons.size()));
        for (auto& button : divideRegionButtons)
            button.setBounds(
                waveEditRow.removeFromLeft(
                    juce::jmin(divisionButtonWidth, waveEditRow.getWidth()))
                    .reduced(2, 3));

        // Slice preparation belongs directly below the waveform in EDIT.
        area.removeFromTop(18);
        juce::Rectangle<int> dualAssistedArea;
        if (dualMonitorLayout)
        {
            auto lowerArea = area;
            area = lowerArea.removeFromLeft(
                (lowerArea.getWidth() - 8) / 2);
            lowerArea.removeFromLeft(8);
            dualAssistedArea = lowerArea;
        }
        // In the dual layout CREATE / VOICES is a two-column module. Derive
        // its row height from the real monitor height so the controls expand
        // into the available space and still contract safely at 850 px.
        const auto dualCreateRowHeight = dualMonitorLayout
            ? juce::jlimit(34, 52, (area.getHeight() - 24) / 12)
            : 0;
        const auto placeEqual = [] (
            juce::Rectangle<int> row,
            std::initializer_list<juce::Component*> components)
        {
            auto remaining = static_cast<int>(components.size());
            for (auto* component : components)
            {
                const auto width = remaining > 1
                    ? row.getWidth() / remaining : row.getWidth();
                component->setBounds(row.removeFromLeft(width).reduced(2, 3));
                --remaining;
            }
        };

        auto sliceRow = area.removeFromTop(
            dualMonitorLayout ? dualCreateRowHeight : 46);
        if (dualMonitorLayout)
        {
            placeEqual(sliceRow, {
                &sliceEditorLabel, &sliceSource, &sliceIndex, &sliceStart,
                &sliceEnd, &setSlice, &playSlice});
        }
        else
        {
            sliceEditorLabel.setBounds(sliceRow.removeFromLeft(82).reduced(3));
            sliceSource.setBounds(sliceRow.removeFromLeft(122).reduced(3));
            sliceIndex.setBounds(sliceRow.removeFromLeft(74).reduced(3));
            sliceStart.setBounds(sliceRow.removeFromLeft(180).reduced(3));
            sliceEnd.setBounds(sliceRow.removeFromLeft(180).reduced(3));
            setSlice.setBounds(sliceRow.removeFromLeft(78).reduced(3));
            playSlice.setBounds(sliceRow.removeFromLeft(
                juce::jmin(120, sliceRow.getWidth())).reduced(3));
        }

        auto orderRow = area.removeFromTop(
            dualMonitorLayout ? dualCreateRowHeight : 44);
        orderLabel.setBounds(orderRow.removeFromLeft(68).reduced(3));
        std::array<juce::TextButton*, 8> orderButtons {
            &randomA, &randomB, &randomAB, &interleave,
            &forwardOrder, &reverseOrder, &zeroOrder, &gapOrder};
        const auto orderButtonWidth =
            orderRow.getWidth() / static_cast<int>(orderButtons.size());
        for (auto* button : orderButtons)
            button->setBounds(
                orderRow.removeFromLeft(orderButtonWidth).reduced(2, 3));

        if (dualMonitorLayout)
        {
            placeEqual(area.removeFromTop(dualCreateRowHeight), {
                &gestureLabel, &gestureStep, &memoryToggle,
                &mutationAmount, &erosionAmount, &deconstructAmount});
            placeEqual(area.removeFromTop(dualCreateRowHeight), {
                &commitTransform, &restoreTransform, &stutter,
                &burst, &micro, &reverseSlice});
        }
        else
        {
            auto gestureRow = area.removeFromTop(48);
            gestureLabel.setBounds(gestureRow.removeFromLeft(75).reduced(3));
            gestureStep.setBounds(gestureRow.removeFromLeft(90).reduced(3));
            memoryToggle.setBounds(gestureRow.removeFromLeft(82).reduced(3));
            mutationAmount.setBounds(gestureRow.removeFromLeft(125).reduced(3));
            erosionAmount.setBounds(gestureRow.removeFromLeft(125).reduced(3));
            deconstructAmount.setBounds(gestureRow.removeFromLeft(125).reduced(3));
            commitTransform.setBounds(gestureRow.removeFromLeft(72).reduced(3));
            restoreTransform.setBounds(gestureRow.removeFromLeft(78).reduced(3));
            stutter.setBounds(gestureRow.removeFromLeft(92).reduced(3));
            burst.setBounds(gestureRow.removeFromLeft(82).reduced(3));
            micro.setBounds(gestureRow.removeFromLeft(82).reduced(3));
            reverseSlice.setBounds(gestureRow.removeFromLeft(110).reduced(3));
        }
        auto formRow = area.removeFromTop(
            dualMonitorLayout ? dualCreateRowHeight : 44);
        if (dualMonitorLayout)
        {
            placeEqual(formRow, {
                &formLabel, &formScene, &formEnable, &formHold, &formNext,
                &formReset, &formBars, &formEnergy, &formVariation});
        }
        else
        {
            formLabel.setBounds(formRow.removeFromLeft(55).reduced(3));
            formScene.setBounds(formRow.removeFromLeft(150).reduced(3));
            formEnable.setBounds(formRow.removeFromLeft(95).reduced(3));
            formHold.setBounds(formRow.removeFromLeft(70).reduced(3));
            formNext.setBounds(formRow.removeFromLeft(70).reduced(3));
            formReset.setBounds(formRow.removeFromLeft(70).reduced(3));
            formBars.setBounds(formRow.removeFromLeft(140).reduced(3));
            formEnergy.setBounds(formRow.removeFromLeft(155).reduced(3));
            formVariation.setBounds(formRow.reduced(3));
        }
        const auto traceHeight = dualMonitorLayout
            ? juce::jmax(
                dualCreateRowHeight + 64,
                area.getHeight() - 3 * dualCreateRowHeight - 4)
            : 176;
        auto tracePanel = area.removeFromTop(traceHeight);
        auto traceRow = tracePanel.removeFromTop(
            dualMonitorLayout ? dualCreateRowHeight : 44);
        traceLabel.setBounds(traceRow.removeFromLeft(75).reduced(3));
        traceRecord.setBounds(traceRow.removeFromLeft(125).reduced(3));
        traceLoop.setBounds(traceRow.removeFromLeft(110).reduced(3));
        traceClear.setBounds(traceRow.removeFromLeft(80).reduced(3));
        traceInfo.setBounds(traceRow.reduced(3));
        tracePad.setBounds(tracePanel.reduced(3, 1));
        if (dualMonitorLayout)
        {
            placeEqual(area.removeFromTop(dualCreateRowHeight), {
                &formTransition, &formBankA, &formBankB,
                &formDensity, &formTension});
            placeEqual(area.removeFromTop(dualCreateRowHeight), {
                &formStability, &formLock, &formAdd, &formDuplicate});
            placeEqual(area.removeFromTop(dualCreateRowHeight), {
                &formContinuity, &formContrast, &formStereoMotion,
                &formDelete, &formMoveUp, &formMoveDown});
            area = dualAssistedArea;
        }
        else
        {
            auto formAdvancedRow = area.removeFromTop(44);
            formTransition.setBounds(
                formAdvancedRow.removeFromLeft(125).reduced(3));
            formBankA.setBounds(formAdvancedRow.removeFromLeft(110).reduced(3));
            formBankB.setBounds(formAdvancedRow.removeFromLeft(110).reduced(3));
            formDensity.setBounds(formAdvancedRow.removeFromLeft(150).reduced(3));
            formTension.setBounds(formAdvancedRow.removeFromLeft(150).reduced(3));
            formStability.setBounds(formAdvancedRow.removeFromLeft(150).reduced(3));
            formLock.setBounds(formAdvancedRow.removeFromLeft(70).reduced(3));
            formAdd.setBounds(formAdvancedRow.removeFromLeft(60).reduced(3));
            formDuplicate.setBounds(formAdvancedRow.removeFromLeft(92).reduced(3));
            auto formCharacterRow = area.removeFromTop(44);
            formContinuity.setBounds(
                formCharacterRow.removeFromLeft(190).reduced(3));
            formContrast.setBounds(
                formCharacterRow.removeFromLeft(190).reduced(3));
            formStereoMotion.setBounds(
                formCharacterRow.removeFromLeft(190).reduced(3));
            formDelete.setBounds(
                formCharacterRow.removeFromLeft(85).reduced(3));
            formMoveUp.setBounds(
                formCharacterRow.removeFromLeft(55).reduced(3));
            formMoveDown.setBounds(
                formCharacterRow.removeFromLeft(55).reduced(3));
        }
        if (dualMonitorLayout)
        {
            placeEqual(area.removeFromTop(dualCreateRowHeight), {
                &assistedLabel, &assistedEnable, &assistedRepeat,
                &assistedSource, &assistedOrder});
            placeEqual(area.removeFromTop(dualCreateRowHeight), {
                &assistedRegion, &assistedTransform, &assistedGaps,
                &assistedPitch, &assistedFragments});
            placeEqual(area.removeFromTop(dualCreateRowHeight), {
                &assistedCuts, &assistedMix, &assistedNext,
                &assistedKeep, &assistedRestore});
            placeEqual(area.removeFromTop(dualCreateRowHeight), {
                &assistedSeed, &assistedApplySeed, &assistedRewind});
            placeEqual(area.removeFromTop(dualCreateRowHeight), {
                &assistedMinBpm, &assistedMaxBpm, &assistedVariation});

            auto motifSlotsRow = area.removeFromTop(dualCreateRowHeight);
            motifLabel.setBounds(
                motifSlotsRow.removeFromLeft(100).reduced(2, 3));
            const auto slotWidth = motifSlotsRow.getWidth()
                / static_cast<int>(motifSlotButtons.size());
            for (auto& slot : motifSlotButtons)
                slot.setBounds(
                    motifSlotsRow.removeFromLeft(slotWidth).reduced(2, 3));
            placeEqual(area.removeFromTop(dualCreateRowHeight), {
                &motifName, &motifCapture, &motifRecall,
                &motifVary, &motifDelete, &motifInfo});
        }
        else
        {
            auto assistedActionRow = area.removeFromTop(44);
            assistedLabel.setBounds(
                assistedActionRow.removeFromLeft(80).reduced(3));
            assistedEnable.setBounds(
                assistedActionRow.removeFromLeft(70).reduced(3));
            assistedRepeat.setBounds(
                assistedActionRow.removeFromLeft(90).reduced(3));
            assistedSource.setBounds(
                assistedActionRow.removeFromLeft(85).reduced(3));
            assistedOrder.setBounds(
                assistedActionRow.removeFromLeft(90).reduced(3));
            assistedRegion.setBounds(
                assistedActionRow.removeFromLeft(85).reduced(3));
            assistedTransform.setBounds(
                assistedActionRow.removeFromLeft(110).reduced(3));
            assistedGaps.setBounds(
                assistedActionRow.removeFromLeft(75).reduced(3));
            assistedPitch.setBounds(
                assistedActionRow.removeFromLeft(75).reduced(3));
            assistedFragments.setBounds(
                assistedActionRow.removeFromLeft(110).reduced(3));
            assistedCuts.setBounds(
                assistedActionRow.removeFromLeft(70).reduced(3));
            assistedMix.setBounds(
                assistedActionRow.removeFromLeft(65).reduced(3));
            assistedNext.setBounds(
                assistedActionRow.removeFromLeft(72).reduced(3));
            assistedKeep.setBounds(
                assistedActionRow.removeFromLeft(72).reduced(3));
            assistedRestore.setBounds(assistedActionRow.reduced(3));

            auto assistedSettingsRow = area.removeFromTop(44);
            assistedSeed.setBounds(
                assistedSettingsRow.removeFromLeft(210).reduced(3));
            assistedApplySeed.setBounds(
                assistedSettingsRow.removeFromLeft(110).reduced(3));
            assistedRewind.setBounds(
                assistedSettingsRow.removeFromLeft(90).reduced(3));
            assistedMinBpm.setBounds(
                assistedSettingsRow.removeFromLeft(245).reduced(3));
            assistedMaxBpm.setBounds(
                assistedSettingsRow.removeFromLeft(245).reduced(3));
            assistedVariation.setBounds(assistedSettingsRow.reduced(3));

            auto motifRow = area.removeFromTop(44);
            motifLabel.setBounds(motifRow.removeFromLeft(115).reduced(3));
            for (auto& slot : motifSlotButtons)
                slot.setBounds(motifRow.removeFromLeft(44).reduced(2, 3));
            motifName.setBounds(motifRow.removeFromLeft(170).reduced(3));
            motifCapture.setBounds(motifRow.removeFromLeft(82).reduced(3));
            motifRecall.setBounds(motifRow.removeFromLeft(76).reduced(3));
            motifVary.setBounds(motifRow.removeFromLeft(64).reduced(3));
            motifDelete.setBounds(motifRow.removeFromLeft(72).reduced(3));
            motifInfo.setBounds(motifRow.reduced(3));
        }

        auto assistedLocksRow = area.removeFromTop(
            dualMonitorLayout ? dualCreateRowHeight : 44);
        const auto lockWidth = assistedLocksRow.getWidth() / 8;
        for (auto* lock : {
                 &lockSource, &lockCuts, &lockPattern, &lockTransform,
                 &lockPitch, &lockGap, &lockMix, &lockVoices})
            lock->setBounds(
                assistedLocksRow.removeFromLeft(lockWidth).reduced(3));

        mixerHeaderLabel.setBounds(mixerRail.removeFromTop(26));
        auto mixerSources = mixerRail.removeFromTop(20);
        const auto mixerLevelHeading = mixerSources.withSizeKeepingCentre(
            juce::jmin(72, mixerSources.getWidth() / 4),
            mixerSources.getHeight());
        mixerLevelLabel.setBounds(mixerLevelHeading);
        mixerSourceLabels[0].setBounds(
            mixerSources.removeFromLeft(mixerSources.getWidth() / 2).reduced(2));
        mixerSourceLabels[1].setBounds(mixerSources.reduced(2));
        const auto placeMixerPair =
            [&mixerRail] (juce::Label& label,
                          std::array<juce::Slider, 2>& sliders)
        {
            label.setBounds(mixerRail.removeFromTop(16).reduced(4, 0));
            auto row = mixerRail.removeFromTop(34);
            sliders[0].setBounds(
                row.removeFromLeft(row.getWidth() / 2).reduced(2));
            sliders[1].setBounds(row.reduced(2));
        };
        auto mixerLevelRow = mixerRail.removeFromTop(34);
        mixerLevels[0].setBounds(
            mixerLevelRow.removeFromLeft(
                mixerLevelRow.getWidth() / 2).reduced(2));
        mixerLevels[1].setBounds(mixerLevelRow.reduced(2));
        placeMixerPair(mixerPanLabel, mixerPans);
        placeMixerPair(mixerWidthLabel, mixerWidths);
        auto mixerSwitches = mixerRail.removeFromTop(34);
        for (std::size_t source = 0; source < mixerLevels.size(); ++source)
        {
            auto sourceSwitches = mixerSwitches.removeFromLeft(
                mixerSwitches.getWidth()
                / static_cast<int>(mixerLevels.size() - source));
            mixerMutes[source].setBounds(
                sourceSwitches.removeFromLeft(
                    sourceSwitches.getWidth() / 2).reduced(2));
            mixerSolos[source].setBounds(sourceSwitches.reduced(2));
        }
        mixerBalanceLabel.setBounds(
            mixerRail.removeFromTop(16).reduced(4, 0));
        mixerBalance.setBounds(mixerRail.removeFromTop(34).reduced(2));
        masterLabel.setBounds(mixerRail.removeFromTop(16).reduced(4, 0));
        master.setBounds(mixerRail.removeFromTop(34).reduced(2));
        outputTrimLabel.setBounds(mixerRail.removeFromTop(16).reduced(4, 0));
        auto technicalOutputRow = mixerRail.removeFromTop(34);
        outputTrim.setBounds(
            technicalOutputRow.removeFromLeft(
                technicalOutputRow.getWidth() * 2 / 3).reduced(2));
        outputMute.setBounds(technicalOutputRow.reduced(2));

        const auto voicesHeaderHeight = dualMonitorLayout ? 20 : 24;
        const auto voiceControlRowHeight = dualMonitorLayout
            ? dualCreateRowHeight
            : juce::jmax(
                42,
                (area.getHeight() - voicesHeaderHeight - 4) / 4);
        voicesHeaderLabel.setBounds(
            area.removeFromTop(voicesHeaderHeight).reduced(3));
        for (std::size_t voice = 0; voice < voiceEnabled.size(); ++voice)
        {
            auto voiceRow = area.removeFromTop(voiceControlRowHeight);
            if (dualMonitorLayout)
            {
                placeEqual(voiceRow, {
                    &voiceLabels[voice], &voiceEnabled[voice],
                    &voiceSources[voice], &voiceDivisions[voice],
                    &voicePitches[voice], &voiceLevels[voice],
                    &voicePans[voice]});
            }
            else
            {
                voiceLabels[voice].setBounds(
                    voiceRow.removeFromLeft(100).reduced(3));
                voiceEnabled[voice].setBounds(
                    voiceRow.removeFromLeft(80).reduced(3));
                voiceSources[voice].setBounds(
                    voiceRow.removeFromLeft(110).reduced(3));
                voiceDivisions[voice].setBounds(
                    voiceRow.removeFromLeft(100).reduced(3));
                voicePitches[voice].setBounds(
                    voiceRow.removeFromLeft(190).reduced(3));
                voiceLevels[voice].setBounds(
                    voiceRow.removeFromLeft(220).reduced(3));
                voicePans[voice].setBounds(voiceRow.reduced(3));
            }
        }
        auto advancedVoiceRow = area.removeFromTop(voiceControlRowHeight);
        if (dualMonitorLayout)
        {
            placeEqual(advancedVoiceRow, {
                &voiceAdvancedLabel, &voiceEditor, &voicePatternLength,
                &voiceFocusStart, &voiceFocusEnd, &voiceAttack,
                &voiceRelease});
        }
        else
        {
            voiceAdvancedLabel.setBounds(
                advancedVoiceRow.removeFromLeft(110).reduced(3));
            voiceEditor.setBounds(
                advancedVoiceRow.removeFromLeft(110).reduced(3));
            voicePatternLength.setBounds(
                advancedVoiceRow.removeFromLeft(155).reduced(3));
            voiceFocusStart.setBounds(
                advancedVoiceRow.removeFromLeft(175).reduced(3));
            voiceFocusEnd.setBounds(
                advancedVoiceRow.removeFromLeft(175).reduced(3));
            voiceAttack.setBounds(
                advancedVoiceRow.removeFromLeft(175).reduced(3));
            voiceRelease.setBounds(advancedVoiceRow.reduced(3));
        }
        auto virtualPatternRow = area.removeFromTop(voiceControlRowHeight);
        voicePatternLabel.setBounds(virtualPatternRow.removeFromLeft(
            dualMonitorLayout ? 90 : 120).reduced(3));
        const auto virtualCellWidth =
            virtualPatternRow.getWidth() / static_cast<int>(voicePatternCells.size());
        for (auto& cell : voicePatternCells)
            cell.setBounds(virtualPatternRow.removeFromLeft(virtualCellWidth).reduced(2));
    }

    [[nodiscard]] PerformanceSnapshot performanceSnapshot() const noexcept
    {
        const auto transport = engine.transportTelemetry();
        return {
            transport.running,
            recorder.isRunning(),
            transport.step,
            // The main XY MOD updates these controls immediately, while the
            // audio-thread telemetry may publish one callback later.  Use
            // the shared UI values here so the detached PERFORM pad mirrors
            // the large pad without a visible lag or stale position.
            tempo.getValue(),
            static_cast<int>(std::lround(pitchSemitones.getValue())),
            transport.currentPattern,
            selectedSliceSource(),
            session.sequencer.timing(),
            session.masterLevel,
            transport.mixerBalance,
            uiAssisted.enabled,
            uiAssisted.repeat,
            uiFormDirector.state().enabled,
            uiFormDirector.state().hold,
            uiFormDirector.state().currentScene};
    }

    void remotePlay()
    {
        submitOrWarn({navalha::EngineCommandType::start});
    }

    void remoteStop()
    {
        submitOrWarn({navalha::EngineCommandType::stop});
    }

    void remoteReset()
    {
        submitOrWarn({navalha::EngineCommandType::reset});
        transportElapsedMilliseconds = 0.0;
    }

    void remoteSetTempo(double bpm)
    {
        tempo.setValue(bpm, juce::sendNotification);
    }

    void remoteSelectPattern(std::size_t index)
    {
        pattern.setSelectedItemIndex(
            static_cast<int>(std::min(index, navalha::patternCount - 1)),
            juce::sendNotification);
    }

    void remoteSetTiming(navalha::TimingMode mode)
    {
        timing.setSelectedItemIndex(
            static_cast<int>(mode), juce::sendNotification);
    }

    void remoteSetPitch(int semitones)
    {
        pitchSemitones.setValue(
            std::clamp(semitones, -12, 11), juce::sendNotification);
    }

    void remoteSetMaster(double level)
    {
        master.setValue(std::clamp(level, 0.0, 1.0), juce::sendNotification);
    }

    void remoteSetAssisted(bool enabled)
    {
        assistedEnable.setToggleState(enabled, juce::dontSendNotification);
        submitAssistedSettings();
    }

    void remoteSetRepeat(bool enabled)
    {
        assistedRepeat.setToggleState(enabled, juce::dontSendNotification);
        submitAssistedSettings();
    }

    void remoteNextAssisted()
    {
        submitOrWarn({navalha::EngineCommandType::forceAssistedDecision});
    }

    void remoteSetSource(std::size_t source)
    {
        source = std::min<std::size_t>(source, 1);
        sliceSource.setSelectedItemIndex(
            static_cast<int>(source), juce::sendNotification);
    }

    void remoteToggleSource()
    {
        remoteSetSource(selectedSliceSource() == 0 ? 1 : 0);
    }

    void remoteSetBalance(double value)
    {
        mixerBalance.setValue(
            std::clamp(value, -1.0, 1.0), juce::sendNotification);
    }

    void remoteApplyPatternMacro(PatternMacro macro)
    {
        applyPatternMacro(macro);
    }

    void remoteStutter() { startStutter(); }

    void remoteBurst()
    {
        submitOrWarn({
            navalha::EngineCommandType::startBurst,
            selectedSliceSource()});
        showStatus("BURST x8");
    }

    void remoteMicro() { appendMicroSlices(); }

    void remoteReverse()
    {
        submitOrWarn({
            navalha::EngineCommandType::triggerSlice,
            selectedSliceSource(), selectedSliceIndex(), 1.0});
        showStatus("REVERSE SLICE");
    }

    void remoteCommitTransform()
    {
        if (!uiPatternTransform.hasBase)
        {
            showStatus("COMMIT | NO ACTIVE TRANSFORM");
            return;
        }
        commitTransformState();
    }
    void remoteRestoreTransform()
    {
        if (!uiPatternTransform.hasBase)
        {
            showStatus("RESTORE | NO ACTIVE TRANSFORM");
            return;
        }
        restoreTransformState();
    }
    void remoteToggleForm() { formEnable.triggerClick(); }
    void remoteToggleFormHold() { formHold.triggerClick(); }
    void remoteNextForm() { formNext.triggerClick(); }
    void remoteResetForm() { formReset.triggerClick(); }
    void remoteToggleRecording() { record.triggerClick(); }

    void setUiLanguage(navalha::ui::Language language)
    {
        uiLanguage = language;
        updateLibrarySearchPlaceholder();
        if (auto* settings = applicationProperties.getUserSettings())
        {
            settings->setValue(
                "uiLanguage", navalha::ui::languageCode(uiLanguage));
            static_cast<void>(settings->saveIfNeeded());
        }
        if (activeLearnKey.isNotEmpty())
            explainLearnKey(activeLearnKey);
        else
            showDefaultLearnText();
    }

    [[nodiscard]] navalha::ui::Language getUiLanguage() const noexcept
    {
        return uiLanguage;
    }

    void setLearningMode(bool enabled)
    {
        learningMode = enabled;
        learnModeLabel.setVisible(enabled);
        learnTitle.setVisible(enabled);
        learnBody.setVisible(enabled);
        if (!enabled)
            activeLearnKey.clear();
        showDefaultLearnText();
        if (auto* settings = applicationProperties.getUserSettings())
        {
            settings->setValue("learnMode", enabled);
            static_cast<void>(settings->saveIfNeeded());
        }
        resized();
        repaint();
    }

    [[nodiscard]] bool isLearningMode() const noexcept
    {
        return learningMode;
    }

    void setVisibleViewportArea(juce::Rectangle<int> newArea)
    {
        if (newArea == visibleViewportArea)
            return;
        visibleViewportArea = newArea;
        resized();
        repaint();
    }

    void explainLearnKey(const juce::String& key)
    {
        if (!learningMode)
            return;
        const auto* entry = navalha::ui::findLearnEntry(key.toStdString());
        if (entry == nullptr)
            return;
        activeLearnKey = key;
        learnTitle.setText(
            navalha::ui::text(entry->title, uiLanguage),
            juce::dontSendNotification);
        learnBody.setText(
            navalha::ui::text(entry->body, uiLanguage), false);
    }

    void setDualMonitorLayout(bool enabled)
    {
        if (dualMonitorLayout == enabled)
            return;
        dualMonitorLayout = enabled;
        resized();
        repaint();
    }

    [[nodiscard]] bool prefersDualMonitor()
    {
        if (auto* settings = applicationProperties.getUserSettings())
            return settings->getBoolValue("dualMonitorEnabled", true);
        return true;
    }

    void setPrefersDualMonitor(bool enabled)
    {
        if (auto* settings = applicationProperties.getUserSettings())
        {
            settings->setValue("dualMonitorEnabled", enabled);
            static_cast<void>(settings->saveIfNeeded());
        }
    }

    [[nodiscard]] const std::vector<navalha::TakeEntry>& takes() const noexcept
    {
        return takeCatalog.entries();
    }

    [[nodiscard]] const navalha::TakeEntry* take(std::string_view id) const noexcept
    {
        return takeCatalog.find(id);
    }

    bool updateTake(navalha::TakeEntry entry)
    {
        try
        {
            takeCatalog.upsert(std::move(entry));
            saveTakeCatalog();
            showStatus("TAKE METADATA SAVED");
            return true;
        }
        catch (const std::exception& exception)
        {
            showStatus("TAKE SAVE FAILED | " + juce::String(exception.what()));
            return false;
        }
    }

    bool useTakeAsSource(std::string_view id, std::size_t sourceIndex)
    {
        const auto* entry = takeCatalog.find(id);
        if (entry == nullptr || sourceIndex > 1)
            return false;
        const juce::File file(utf8(entry->audioPath));
        if (!file.existsAsFile())
        {
            showStatus("TAKE AUDIO MISSING | " + file.getFileName());
            return false;
        }
        loadSource(sourceIndex, file);
        showStatus("TAKE → SOURCE "
                   + juce::String(sourceIndex == 0 ? "A | " : "B | ")
                   + file.getFileName());
        return true;
    }

    std::pair<int, int> importTakeDirectory(const juce::File& directory)
    {
        if (!directory.isDirectory())
            return {0, 1};
        juce::Array<juce::File> files;
        directory.findChildFiles(
            files, juce::File::findFiles, true, "*.wav;*.wave");
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        int imported = 0;
        int errors = 0;
        for (const auto& file : files)
        {
            if (takeCatalog.entries().size()
                >= navalha::maximumTakeCatalogEntries)
                break;
            const auto path = file.getFullPathName().toStdString();
            const auto alreadyPresent = std::any_of(
                takeCatalog.entries().begin(), takeCatalog.entries().end(),
                [&path] (const auto& entry) { return entry.audioPath == path; });
            if (alreadyPresent)
                continue;
            auto reader = std::unique_ptr<juce::AudioFormatReader>(
                formats.createReaderFor(file));
            if (reader == nullptr || reader->sampleRate <= 0.0
                || reader->lengthInSamples <= 0)
            {
                ++errors;
                continue;
            }
            try
            {
                auto created = file.getCreationTime();
                if (created.toMilliseconds() <= 0)
                    created = file.getLastModificationTime();
                navalha::TakeEntry entry;
                const auto fullPath = file.getFullPathName();
                std::uint64_t identity = 14695981039346656037ULL;
                for (const auto* character = fullPath.toRawUTF8();
                     *character != '\0'; ++character)
                {
                    identity ^= static_cast<std::uint8_t>(*character);
                    identity *= 1099511628211ULL;
                }
                entry.id = ("import-" + juce::String::toHexString(
                    static_cast<juce::int64>(identity))).toStdString();
                entry.audioPath = path;
                entry.filename = file.getFileName().toStdString();
                entry.createdAt = created.toISO8601(true).toStdString();
                entry.frames = static_cast<std::uint64_t>(reader->lengthInSamples);
                entry.sampleRate = static_cast<std::uint32_t>(
                    std::lround(reader->sampleRate));
                entry.durationSeconds = static_cast<double>(entry.frames)
                    / reader->sampleRate;
                entry.sampleFormat = reader->bitsPerSample <= 16
                    ? navalha::WavSampleFormat::pcm16
                    : reader->bitsPerSample <= 24
                        ? navalha::WavSampleFormat::pcm24
                        : navalha::WavSampleFormat::float32;
                const auto metadataValue = [&reader]
                    (std::initializer_list<const char*> keys)
                {
                    for (const auto* key : keys)
                    {
                        const auto value = reader->metadataValues[key].trim();
                        if (value.isNotEmpty())
                            return value.toStdString();
                    }
                    return std::string {};
                };
                entry.metadata = {
                    metadataValue({"INAM", "Title", "title"}),
                    metadataValue({"IART", "Artist", "artist"}),
                    metadataValue({"IPRD", "Album", "project"}),
                    metadataValue({"ICRD", "Year", "date"}),
                    metadataValue({"ICMT", "Comment", "comment",
                                   "bwavDescription"})};
                takeCatalog.upsert(std::move(entry));
                ++imported;
            }
            catch (...)
            {
                ++errors;
            }
        }
        saveTakeCatalog();
        showStatus(
            "TAKE IMPORT | " + juce::String(imported) + " ADDED | "
            + juce::String(errors) + " ERRORS");
        return {imported, errors};
    }

private:
    void initialiseUiPreferences()
    {
        if (auto* settings = applicationProperties.getUserSettings())
        {
            uiLanguage = navalha::ui::languageFromCode(
                settings->getValue("uiLanguage", "en"));
            learningMode = settings->getBoolValue("learnMode", false);
        }
        learnModeLabel.setVisible(learningMode);
        learnTitle.setVisible(learningMode);
        learnBody.setVisible(learningMode);
        updateLibrarySearchPlaceholder();
        showDefaultLearnText();
    }

    void updateLibrarySearchPlaceholder()
    {
        librarySearch.setTextToShowWhenEmpty(
            navalha::ui::text(
                {"SEARCH FILES...", "BUSCAR ARQUIVOS...",
                 "RECHERCHER DES FICHIERS...", "BUSCAR ARCHIVOS..."},
                uiLanguage),
            juce::Colour(Arcade::muted));
    }

    void showDefaultLearnText()
    {
        const navalha::ui::LocalizedText titleText {
            "Move over a control", "Passe sobre um controle",
            "Passez sur un contrôle", "Pase sobre un control"};
        const navalha::ui::LocalizedText bodyText {
            "Its function will be explained here without covering the instrument.",
            "Sua função será explicada aqui sem cobrir o instrumento.",
            "Sa fonction sera expliquée ici sans couvrir l’instrument.",
            "Su función se explicará aquí sin cubrir el instrumento."};
        learnModeLabel.setText(
            navalha::ui::text({"LEARNING MODE", "MODO DE APRENDIZAGEM",
                               "MODE D’APPRENTISSAGE", "MODO DE APRENDIZAJE"},
                              uiLanguage),
            juce::dontSendNotification);
        learnTitle.setText(
            navalha::ui::text(titleText, uiLanguage),
            juce::dontSendNotification);
        learnBody.setText(navalha::ui::text(bodyText, uiLanguage), false);
    }

    static void registerLearn(juce::Component& component,
                              const char* key)
    {
        jassert(navalha::ui::findLearnEntry(key) != nullptr);
        component.getProperties().set("learnKey", key);
    }

    void configureLearningMetadata()
    {
        registerLearn(title, "app");
        registerLearn(audioConnectionStatus, "audio");
        registerLearn(status, "audio");
        registerLearn(openProject, "openproject");
        registerLearn(saveProject, "saveproject");
        registerLearn(savePortable, "saveportable");
        registerLearn(play, "play");
        registerLearn(stop, "stop");
        registerLearn(resetTransport, "reset");
        registerLearn(record, "rec");
        registerLearn(transportClock, "transportclock");
        registerLearn(transportInfo, "transportclock");
        registerLearn(audioLibrary, "library");
        registerLearn(libraryLabel, "library");
        registerLearn(libraryPath, "library");
        registerLearn(libraryHint, "library");
        registerLearn(selectedLabel, "library");
        registerLearn(selectedInfo, "library");
        registerLearn(chooseLibraryFolder, "library");
        registerLearn(librarySearch, "librarysearch");
        registerLearn(loadSelectedA, "library");
        registerLearn(loadSelectedB, "library");
        registerLearn(previewSelected, "librarypreview");
        registerLearn(stopPreview, "librarypreview");
        registerLearn(logLabel, "activitylog");
        registerLearn(activityLog, "activitylog");
        registerLearn(copyLog, "activitylog");
        registerLearn(clearLog, "activitylog");
        registerLearn(learnModeLabel, "learn");
        registerLearn(learnTitle, "learn");
        registerLearn(learnBody, "learn");
        registerLearn(waveform, "waveform");
        registerLearn(waveEditLabel, "waveform");
        registerLearn(selectRegionMode, "selectregion");
        registerLearn(editSliceMode, "editslice");
        registerLearn(bladeMode, "blade");
        registerLearn(wholeRegion, "selectregion");
        registerLearn(undoBlade, "blade");
        registerLearn(playSlice, "editslice");
        registerLearn(setSlice, "editslice");
        registerLearn(sliceEditorLabel, "slice");
        registerLearn(sliceSource, "slice");
        registerLearn(sliceIndex, "slice");
        registerLearn(sliceStart, "slice");
        registerLearn(sliceEnd, "slice");
        registerLearn(divideRegionLabel, "divide");
        for (auto& button : divideRegionButtons)
            registerLearn(button, "divide");
        registerLearn(tempo, "bpm");
        registerLearn(tempoLabel, "bpm");
        registerLearn(division, "timing");
        registerLearn(divisionLabel, "timing");
        registerLearn(pattern, "pattern");
        registerLearn(patternLabel, "pattern");
        registerLearn(patternCellsLabel, "pattern");
        for (auto& cell : patternCells)
            registerLearn(cell, "pattern");
        registerLearn(timing, "timing");
        registerLearn(timingLabel, "timing");
        registerLearn(jitterControl, "timing");
        registerLearn(jitterLabel, "timing");
        registerLearn(timingSeedEditor, "timing");
        registerLearn(timingSeedLabel, "timing");
        registerLearn(applyTimingSeed, "timing");
        registerLearn(pitchSemitones, "pitch");
        registerLearn(pitchLabel, "pitch");
        registerLearn(pitchMix, "pitch");
        registerLearn(pitchMixLabel, "pitch");
        registerLearn(pitchBypass, "pitch");
        registerLearn(pitchZero, "pitch");
        registerLearn(pitchAudition, "pitch");
        for (auto* button : {
                 &randomA, &randomB, &randomAB, &interleave, &forwardOrder,
                 &reverseOrder, &zeroOrder, &gapOrder})
            registerLearn(*button, "order");
        registerLearn(orderLabel, "order");
        registerLearn(gestureLabel, "gesture");
        registerLearn(gestureStep, "gesture");
        registerLearn(memoryToggle, "gesture");
        registerLearn(mutationAmount, "gesture");
        registerLearn(erosionAmount, "gesture");
        registerLearn(deconstructAmount, "gesture");
        for (auto* button : {
                 &commitTransform, &restoreTransform, &stutter, &burst,
                 &micro, &reverseSlice})
            registerLearn(*button, "gesture");
        registerLearn(assistedEnable, "assisted");
        registerLearn(assistedLabel, "assisted");
        for (auto* toggle : {
                 &assistedRepeat, &assistedSource, &assistedOrder,
                 &assistedRegion, &assistedCuts, &assistedMix,
                 &assistedTransform, &assistedGaps, &assistedPitch,
                 &assistedFragments})
            registerLearn(*toggle, "assisted");
        for (auto* control : {
                 static_cast<juce::Component*>(&assistedMinBpm),
                 static_cast<juce::Component*>(&assistedMaxBpm),
                 static_cast<juce::Component*>(&assistedVariation),
                 static_cast<juce::Component*>(&assistedSeed),
                 static_cast<juce::Component*>(&assistedApplySeed),
                 static_cast<juce::Component*>(&assistedRewind),
                 static_cast<juce::Component*>(&assistedNext),
                 static_cast<juce::Component*>(&assistedKeep),
                 static_cast<juce::Component*>(&assistedRestore)})
            registerLearn(*control, "assisted");
        registerLearn(formLabel, "form");
        registerLearn(formEnable, "form");
        for (auto* control : {
                 static_cast<juce::Component*>(&formScene),
                 static_cast<juce::Component*>(&formHold),
                 static_cast<juce::Component*>(&formNext),
                 static_cast<juce::Component*>(&formReset),
                 static_cast<juce::Component*>(&formBars),
                 static_cast<juce::Component*>(&formEnergy),
                 static_cast<juce::Component*>(&formVariation),
                 static_cast<juce::Component*>(&formTransition),
                 static_cast<juce::Component*>(&formBankA),
                 static_cast<juce::Component*>(&formBankB),
                 static_cast<juce::Component*>(&formDensity),
                 static_cast<juce::Component*>(&formTension),
                 static_cast<juce::Component*>(&formStability),
                 static_cast<juce::Component*>(&formContinuity),
                 static_cast<juce::Component*>(&formContrast),
                 static_cast<juce::Component*>(&formStereoMotion),
                 static_cast<juce::Component*>(&formLock),
                 static_cast<juce::Component*>(&formAdd),
                 static_cast<juce::Component*>(&formDuplicate),
                 static_cast<juce::Component*>(&formDelete),
                 static_cast<juce::Component*>(&formMoveUp),
                 static_cast<juce::Component*>(&formMoveDown)})
            registerLearn(*control, "form");
        registerLearn(tracePad, "trace");
        registerLearn(traceLabel, "trace");
        registerLearn(traceInfo, "trace");
        registerLearn(traceRecord, "trace");
        registerLearn(traceLoop, "trace");
        registerLearn(traceClear, "trace");
        registerLearn(motifLabel, "motif");
        for (auto& button : motifSlotButtons)
            registerLearn(button, "motif");
        registerLearn(motifName, "motif");
        registerLearn(motifInfo, "motif");
        for (auto* button : {
                 &motifCapture, &motifRecall, &motifVary, &motifDelete})
            registerLearn(*button, "motif");
        for (auto* toggle : {
                 &lockSource, &lockCuts, &lockPattern, &lockTransform,
                 &lockPitch, &lockGap, &lockMix, &lockVoices})
            registerLearn(*toggle, "motif");
        registerLearn(mixerHeaderLabel, "mixer");
        registerLearn(mixerLevelLabel, "mixerlevel");
        registerLearn(mixerPanLabel, "mixerpan");
        registerLearn(mixerWidthLabel, "mixerwidth");
        registerLearn(mixerBalanceLabel, "mixerbalance");
        registerLearn(mixerBalance, "mixerbalance");
        for (std::size_t source = 0; source < 2; ++source)
        {
            registerLearn(mixerSourceLabels[source], "mixerlevel");
            registerLearn(mixerLevels[source], "mixerlevel");
            registerLearn(mixerPans[source], "mixerpan");
            registerLearn(mixerWidths[source], "mixerwidth");
            registerLearn(mixerMutes[source], "mixermute");
            registerLearn(mixerSolos[source], "mixersolo");
        }
        registerLearn(master, "output");
        registerLearn(masterLabel, "output");
        registerLearn(outputTrim, "output");
        registerLearn(outputTrimLabel, "output");
        registerLearn(outputMute, "output");
        registerLearn(outputMeterLabel, "output");
        registerLearn(outputLeftMeter, "output");
        registerLearn(outputRightMeter, "output");
        registerLearn(recordingFormat, "recordformat");
        registerLearn(recordingFormatLabel, "recordformat");
        registerLearn(recordingInfo, "recordformat");
        registerLearn(voicesHeaderLabel, "voices");
        registerLearn(voiceAdvancedLabel, "voices");
        registerLearn(voicePatternLabel, "voices");
        registerLearn(voiceEditor, "voices");
        registerLearn(voicePatternLength, "voices");
        registerLearn(voiceFocusStart, "voices");
        registerLearn(voiceFocusEnd, "voices");
        registerLearn(voiceAttack, "voices");
        registerLearn(voiceRelease, "voices");
        for (auto& cell : voicePatternCells)
            registerLearn(cell, "voices");
        for (std::size_t voice = 0; voice < 2; ++voice)
        {
            registerLearn(voiceLabels[voice], "voices");
            registerLearn(voiceEnabled[voice], "voices");
            registerLearn(voiceSources[voice], "voices");
            registerLearn(voiceDivisions[voice], "voices");
            registerLearn(voicePitches[voice], "voices");
            registerLearn(voiceLevels[voice], "voices");
            registerLearn(voicePans[voice], "voices");
        }
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        if (!learningMode)
            return;
        auto* component = event.originalComponent;
        while (component != nullptr && component != this)
        {
            if (component->getProperties().contains("learnKey"))
            {
                explainLearnKey(
                    component->getProperties()["learnKey"].toString());
                return;
            }
            component = component->getParentComponent();
        }
    }

    void globalFocusChanged(juce::Component* focusedComponent) override
    {
        if (!learningMode)
            return;
        auto* component = focusedComponent;
        while (component != nullptr)
        {
            if (component->getProperties().contains("learnKey"))
            {
                explainLearnKey(
                    component->getProperties()["learnKey"].toString());
                return;
            }
            component = component->getParentComponent();
        }
    }

    void refreshLibraryHint()
    {
        libraryHint.setText(
            juce::String(audioLibrary.fileCount())
                + " FILES | DRAG WAV/AIFF TO SOURCE A/B",
            juce::dontSendNotification);
    }

    void startAudioPreview(const juce::File& file)
    {
        stopAudioPreview(false);
        auto* reader = previewFormatManager.createReaderFor(file);
        if (reader == nullptr)
        {
            showStatus("PREVIEW FAILED | " + file.getFileName());
            return;
        }
        const auto sourceRate = reader->sampleRate;
        previewReader = std::make_unique<juce::AudioFormatReaderSource>(
            reader, true);
        previewTransport.setSource(
            previewReader.get(), 0, nullptr, sourceRate);
        previewTransport.setGain(0.70F);
        previewTransport.start();
        showStatus("PREVIEW | " + file.getFileName());
    }

    void stopAudioPreview(bool announce = true)
    {
        const auto wasActive = previewTransport.isPlaying()
            || previewReader != nullptr;
        previewTransport.stop();
        previewTransport.setSource(nullptr);
        previewReader.reset();
        if (announce && wasActive)
            showStatus("PREVIEW STOPPED");
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        saveAudioSettings();
        refreshAudioConnectionStatus();
    }

    void initialiseAudioDevice()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Navalha2JUCE";
        options.filenameSuffix = "settings";
        options.folderName = "Navalha2";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        applicationProperties.setStorageParameters(options);

        std::unique_ptr<juce::XmlElement> savedState;
        if (auto* settings = applicationProperties.getUserSettings())
        {
            savedState = juce::parseXML(settings->getValue("audioDeviceState"));
            const auto savedTrim = juce::jlimit(
                -24.0, 0.0, settings->getDoubleValue("outputTrimDb", 0.0));
            const auto savedMute = settings->getBoolValue(
                "outputMuted", false);
            outputTrim.setValue(savedTrim, juce::dontSendNotification);
            outputMute.setToggleState(savedMute, juce::dontSendNotification);
            static_cast<void>(engine.setOutputTrimDb(
                static_cast<float>(savedTrim)));
            engine.setOutputMuted(savedMute);
            const auto catalogJson = settings->getValue("takeCatalogV1");
            if (catalogJson.isNotEmpty())
            {
                try
                {
                    takeCatalog = navalha::decodeTakeCatalog(
                        catalogJson.toStdString());
                }
                catch (const std::exception& exception)
                {
                    showStatus("TAKE CATALOG IGNORED | "
                               + juce::String(exception.what()));
                }
            }
        }
        setAudioChannels(0, 2, savedState.get());
    }

    void saveTakeCatalog()
    {
        auto* settings = applicationProperties.getUserSettings();
        if (settings == nullptr)
            return;
        try
        {
            settings->setValue(
                "takeCatalogV1",
                utf8(navalha::encodeTakeCatalog(takeCatalog)));
            static_cast<void>(settings->saveIfNeeded());
        }
        catch (const std::exception& exception)
        {
            showStatus("TAKE CATALOG SAVE FAILED | "
                       + juce::String(exception.what()));
        }
    }

    void saveAudioSettings()
    {
        auto state = deviceManager.createStateXml();
        auto* settings = applicationProperties.getUserSettings();
        if (settings != nullptr)
        {
            if (state != nullptr)
                settings->setValue("audioDeviceState", state.get());
            settings->setValue("outputTrimDb", outputTrim.getValue());
            settings->setValue(
                "outputMuted", outputMute.getToggleState());
            static_cast<void>(settings->saveIfNeeded());
        }
    }

    void refreshAudioConnectionStatus()
    {
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            audioConnectionStatus.setButtonText("AUDIO CONNECTED");
            audioConnectionStatus.setColour(
                juce::TextButton::textColourOffId,
                juce::Colour(Arcade::yellowHigh));
            audioConnectionStatus.setColour(
                juce::TextButton::buttonColourId,
                juce::Colour(Arcade::surfaceHigh));
            audioConnectionStatus.setTooltip(
                device->getName() + " | "
                    + juce::String(device->getCurrentSampleRate(), 0)
                    + " Hz | CLICK FOR AUDIO SETUP");
        }
        else
        {
            audioConnectionStatus.setButtonText("AUDIO DISCONNECTED");
            audioConnectionStatus.setColour(
                juce::TextButton::textColourOffId,
                juce::Colour(Arcade::red));
            audioConnectionStatus.setColour(
                juce::TextButton::buttonColourId,
                juce::Colour(0xff241416));
            audioConnectionStatus.setTooltip(
                "No active output | click for AUDIO SETUP");
        }
    }

    void showAudioSetup()
    {
        if (recorder.isRunning())
        {
            showStatus("STOP RECORDING BEFORE AUDIO SETUP");
            return;
        }
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(new AudioSettingsPanel(deviceManager));
        options.dialogTitle = "Navalha 2 | Audio Setup";
        options.dialogBackgroundColour = juce::Colour(0xff171b1b);
        options.componentToCentreAround = this;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.launchAsync();
    }

    void timerCallback() override
    {
        const auto peak = engine.consumeOutputPeak();
        const auto safety = engine.consumeOutputSafetyTelemetry();
        meterLeft = std::max(
            static_cast<double>(std::clamp(peak.left, 0.0F, 1.0F)),
            meterLeft * 0.86);
        meterRight = std::max(
            static_cast<double>(std::clamp(peak.right, 0.0F, 1.0F)),
            meterRight * 0.86);
        const auto dbText = [] (float value)
        {
            if (value <= 1.0e-6F) return juce::String("-inf");
            return juce::String(20.0 * std::log10(value), 1);
        };
        outputLeftMeter.setTextToDisplay(
            "L " + dbText(peak.left) + " dBFS");
        outputRightMeter.setTextToDisplay(
            "R " + dbText(peak.right) + " dBFS");
        if (safety.ceilingEngaged || safety.nonFiniteSamples != 0)
        {
            safetyHoldTicks = 60;
            heldSafetyGainReductionDb = std::max(
                heldSafetyGainReductionDb,
                static_cast<double>(safety.gainReductionDb));
        }
        else if (safetyHoldTicks > 0)
            --safetyHoldTicks;
        if (safetyHoldTicks == 0)
            heldSafetyGainReductionDb = 0.0;
        const auto safetyActive = safetyHoldTicks > 0;
        const auto meterColour = safetyActive
            ? juce::Colour(Arcade::red) : juce::Colour(Arcade::yellow);
        outputLeftMeter.setColour(
            juce::ProgressBar::foregroundColourId, meterColour);
        outputRightMeter.setColour(
            juce::ProgressBar::foregroundColourId, meterColour);
        juce::String outputStatus = "MASTER SAFE";
        if (safety.suspended)
            outputStatus = "RECONNECT SAFE";
        else if (safety.muted)
            outputStatus = "OUTPUT MUTED";
        else if (safety.nonFiniteSamples != 0)
            outputStatus = "INVALID";
        else if (safetyActive)
            outputStatus = "SAFE -" + juce::String(heldSafetyGainReductionDb, 1);
        outputMeterLabel.setText(outputStatus, juce::dontSendNotification);
        outputMeterLabel.setTooltip(
            "Post-safety sample peak and block RMS. Input peak: "
            + dbText(safety.inputSamplePeak) + " dBFS | RMS L/R: "
            + dbText(safety.rms.left) + " / " + dbText(safety.rms.right)
            + " dBFS | true peak in/out: "
            + dbText(safety.inputTruePeak) + " / "
            + dbText(safety.outputTruePeak) + " dBTP"
            + " | GR: " + juce::String(safety.gainReductionDb, 1)
            + " dB | output trim: "
            + juce::String(safety.outputTrimDb, 1) + " dB"
            + (safety.suspended ? " | reconnect suspended"
                                : (safety.muted ? " | muted" : ""))
            + ". True peak 4x; linked lookahead 5 ms; EBU 15-23 passed "
              "(20-23 derived); official WAV cross-check pending.");
        outputLeftMeter.repaint();
        outputRightMeter.repaint();

        const auto transport = engine.transportTelemetry();
        const auto nowMilliseconds = juce::Time::getMillisecondCounterHiRes();
        if (transport.running && !displayedTransportRunning)
            transportStartedAtMilliseconds =
                nowMilliseconds - transportElapsedMilliseconds;
        else if (!transport.running && displayedTransportRunning)
            transportElapsedMilliseconds =
                nowMilliseconds - transportStartedAtMilliseconds;
        const auto visibleMilliseconds = transport.running
            ? nowMilliseconds - transportStartedAtMilliseconds
            : transportElapsedMilliseconds;
        const auto totalHundredths = static_cast<int>(
            std::max(0.0, visibleMilliseconds) / 10.0);
        transportClock.setText(
            juce::String::formatted(
                "%02d:%02d:%02d",
                totalHundredths / 6000,
                (totalHundredths / 100) % 60,
                totalHundredths % 100),
            juce::dontSendNotification);
        play.setToggleState(transport.running, juce::dontSendNotification);
        stop.setToggleState(!transport.running, juce::dontSendNotification);
        if (transport.tracePlaying)
        {
            tempo.setValue(transport.bpm, juce::dontSendNotification);
            pitchSemitones.setValue(
                transport.pitch, juce::dontSendNotification);
        }
        tracePad.setValues(
            tempo.getValue(),
            static_cast<int>(std::lround(pitchSemitones.getValue())));
        if (uiAssisted.enabled)
        {
            pattern.setSelectedItemIndex(
                static_cast<int>(transport.currentPattern),
                juce::dontSendNotification);
            uiPatterns.setPattern(
                transport.currentPattern, transport.patternRow);
            refreshPatternCells();
            sliceSource.setSelectedItemIndex(
                static_cast<int>(transport.activeSource),
                juce::dontSendNotification);
            uiMixer.balance = transport.mixerBalance;
            uiMixer.sourceA.pan = transport.mixerPan[0];
            uiMixer.sourceB.pan = transport.mixerPan[1];
            uiMixer.sourceA.width = transport.mixerWidth[0];
            uiMixer.sourceB.width = transport.mixerWidth[1];
            syncMixerControls();
        }
        if (traceLooping != transport.tracePlaying && !traceRecording)
        {
            traceLooping = transport.tracePlaying;
            syncTraceControls();
        }
        if (transport.formScene != displayedFormScene
            || transport.formBar != displayedFormBar
            || transport.formCompleted != displayedFormCompleted)
        {
            const auto sceneChanged =
                transport.formScene != displayedFormScene;
            displayedFormScene = transport.formScene;
            displayedFormBar = transport.formBar;
            displayedFormCompleted = transport.formCompleted;
            auto formState = uiFormDirector.state();
            formState.currentScene = std::min(
                transport.formScene, formState.sceneCount - 1);
            formState.bar = transport.formBar;
            formState.completed = transport.formCompleted;
            uiFormDirector.restore(std::move(formState));
            if (sceneChanged)
                applyUiFormSceneMaterial();
            syncFormControls();
        }
        transportInfo.setText(
            transport.running
                ? "TRANSPORT: PLAY | NEXT " + juce::String(transport.step + 1)
                : "TRANSPORT: STOP",
            juce::dontSendNotification);
        if (transport.running != displayedTransportRunning
            || transport.step != displayedTransportStep)
        {
            displayedTransportRunning = transport.running;
            displayedTransportStep = transport.step;
            for (std::size_t step = 0; step < patternCells.size(); ++step)
            {
                const auto active = transport.running && step == transport.step;
                patternCells[step].setColour(
                    juce::Slider::trackColourId,
                    active ? juce::Colour(0xfff1b640)
                           : juce::Colour(0xff56605f));
            }
        }

        const auto isRecording = recorder.isRunning();
        // Keep unattended captures bounded.  The writer also receives the
        // same limit below, while this timer finalizes the take cleanly and
        // gives the user an explicit status message at the five-minute mark.
        constexpr std::uint64_t maxRecordingSeconds = 5ULL * 60ULL;
        const auto recordingRate = activeSampleRate.load(std::memory_order_acquire);
        if (isRecording && recordingRate > 0
            && recorder.framesWritten()
                   >= static_cast<std::uint64_t>(recordingRate)
                          * maxRecordingSeconds)
        {
            finalizeRecording();
            showStatus("RECORDING AUTO-STOP | 5 MIN LIMIT");
            return;
        }
        record.setToggleState(isRecording, juce::dontSendNotification);
        const auto recordText = isRecording
                                            ? juce::String::fromUTF8("REC • STOP")
                                            : juce::String("REC");
        if (record.getButtonText() != recordText)
            record.setButtonText(recordText);
        recordingInfo.setText(
            juce::String(isRecording ? "REC | " : "IDLE | ")
                + juce::String(recorder.framesWritten()) + " FRAMES | "
                + juce::String(engine.droppedRecordingFrames()) + " DROPS | "
                + juce::String(
                    activeSampleRate.load(std::memory_order_acquire), 0) + " Hz",
            juce::dontSendNotification);
        recordingFormat.setEnabled(!recorder.isRunning());
        if (isRecording)
            recorderObservedRunning = true;
        else if (recorderObservedRunning && !recorder.error().empty())
            finalizeRecording();
    }

    void finalizeRecording()
    {
        recorderObservedRunning = false;
        recorder.stop();
        record.setToggleState(false, juce::dontSendNotification);
        record.setButtonText("REC");
        const auto drops = engine.droppedRecordingFrames();
        const auto writerError = recorder.error();
        if (!writerError.empty())
            showStatus("RECORDING FAILED | " + juce::String(writerError));
        else
        {
            try
            {
                registerFinalizedTake();
                if (drops != 0)
                    showStatus(
                        "TAKE FINALIZED | " + juce::String(drops)
                        + " DROPPED FRAMES");
                else
                    showStatus("TAKE FINALIZED | ADDED TO TIMELINE");
            }
            catch (const std::exception& exception)
            {
                showStatus("TAKE CATALOG FAILED | "
                           + juce::String(exception.what()));
            }
        }
        clearActiveRecordingRegistration();
    }

    void configureButton(juce::TextButton& button,
                         const juce::String& text,
                         std::function<void()> action)
    {
        button.setButtonText(text);
        button.onClick = std::move(action);
        addAndMakeVisible(button);
    }

    void configureParameterLabel(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(label);
    }

    void promoteModuleHeading(juce::Label& label, bool alignLeft = false)
    {
        label.setJustificationType(
            alignLeft ? juce::Justification::centredLeft
                      : juce::Justification::centred);
        label.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 10.5F, juce::Font::bold)));
        label.getProperties().set("arcadeFontSize", 10.5);
        label.getProperties().set("arcadeFontBold", true);
        label.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        label.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surfaceHigh));
        label.setColour(
            juce::Label::outlineColourId, juce::Colour(Arcade::line));
    }

    void submitOrWarn(const navalha::EngineCommand& command)
    {
        if (!engine.submitCommand(command))
            showStatus("COMMAND QUEUE FULL OR INVALID VALUE");
    }

    void submitPitch()
    {
        submitOrWarn({
            navalha::EngineCommandType::setHeritagePitch,
            0,
            0,
            pitchSemitones.getValue(),
            pitchMix.getValue()});
        updatePitchModeButtons();
        captureTracePoint();
    }

    void stopTraceLoopFromGesture()
    {
        if (!traceLooping)
            return;
        traceLooping = false;
        submitOrWarn({navalha::EngineCommandType::stopTraceLoop});
        syncTraceControls();
    }

    void beginTracePadGesture()
    {
        stopTraceLoopFromGesture();
        if (!traceArmed)
            return;
        uiControlTrace.clear();
        submitOrWarn({navalha::EngineCommandType::clearControlTrace});
        traceStartedAt = juce::Time::getMillisecondCounterHiRes();
        traceRecording = true;
        syncTraceControls();
    }

    void moveTracePad(double bpm, int pitchValue)
    {
        tempo.setValue(
            std::clamp(bpm, 20.0, 400.0), juce::dontSendNotification);
        pitchSemitones.setValue(
            std::clamp(pitchValue, -12, 11), juce::dontSendNotification);
        submitOrWarn({
            navalha::EngineCommandType::setTempo,
            static_cast<std::size_t>(division.getSelectedItemIndex()),
            0,
            tempo.getValue()});
        submitOrWarn({
            navalha::EngineCommandType::setHeritagePitch,
            0,
            0,
            pitchSemitones.getValue(),
            pitchMix.getValue()});
        updatePitchModeButtons();
        captureTracePoint();
    }

    void endTracePadGesture()
    {
        if (!traceRecording)
            return;
        captureTracePoint(true);
        traceRecording = false;
        traceArmed = false;
        syncTraceControls();
        showStatus(
            "TRACE SAVED | " + juce::String(uiControlTrace.size())
                + " POINTS | "
                + juce::String(uiControlTrace.durationMs() / 1000.0, 1)
                + " S");
    }

    void updatePitchModeButtons()
    {
        const auto heritageEnabled = pitchMix.getValue() > 0.0001;
        pitchBypass.setToggleState(
            heritageEnabled, juce::dontSendNotification);
        pitchBypass.setButtonText(
            heritageEnabled ? "HERITAGE ON" : "HERITAGE OFF");
        pitchZero.setToggleState(
            std::lround(pitchSemitones.getValue()) == 0,
            juce::dontSendNotification);
    }

    void captureTracePoint(bool force = false)
    {
        if (!traceRecording)
            return;
        const auto elapsed = static_cast<std::uint32_t>(std::max(
            0.0, std::round(
                juce::Time::getMillisecondCounterHiRes() - traceStartedAt)));
        const auto bpm = static_cast<int>(std::lround(tempo.getValue()));
        const auto pitchValue =
            static_cast<int>(std::lround(pitchSemitones.getValue()));
        if (!uiControlTrace.append(elapsed, bpm, pitchValue, force))
            return;
        submitOrWarn({
            navalha::EngineCommandType::appendControlTracePoint,
            static_cast<std::size_t>(force), 0,
            static_cast<double>(elapsed),
            static_cast<double>(bpm),
            static_cast<double>(pitchValue)});
        syncTraceControls();
    }

    void syncTraceControls()
    {
        traceRecord.setButtonText(
            traceRecording ? "STOP TRACE"
                : traceArmed ? "TRACE ARMED" : "RECORD TRACE");
        traceLoop.setButtonText(
            traceLooping ? "STOP LOOP" : "TRACE LOOP");
        traceInfo.setText(
            juce::String(uiControlTrace.size()) + " pts | "
                + juce::String(uiControlTrace.durationMs() / 1000.0, 1)
                + " s",
            juce::dontSendNotification);
        tracePad.setTrace(uiControlTrace.points());
    }

    void submitTiming()
    {
        submitOrWarn({
            navalha::EngineCommandType::setTiming,
            static_cast<std::size_t>(
                std::max(0, timing.getSelectedItemIndex())),
            0,
            jitterAmount,
            static_cast<double>(timingSeed)});
    }

    void updateTimingSeed()
    {
        std::uint32_t parsed = 0;
        if (!navalha::AssistedRng::parseSeed(
                timingSeedEditor.getText().toStdString(), parsed))
        {
            showStatus("INVALID TIMING SEED");
            timingSeedEditor.setText(
                juce::String(navalha::AssistedRng::formatSeed(timingSeed)), false);
            return;
        }
        timingSeed = parsed;
        timingSeedEditor.setText(
            juce::String(navalha::AssistedRng::formatSeed(timingSeed)), false);
        submitTiming();
    }

    void submitMixer(std::size_t source)
    {
        auto& channel = source == 0 ? uiMixer.sourceA : uiMixer.sourceB;
        channel.level = mixerLevels[source].getValue();
        channel.pan = mixerPans[source].getValue();
        channel.width = mixerWidths[source].getValue();
        channel.muted = mixerMutes[source].getToggleState();
        channel.solo = mixerSolos[source].getToggleState();
        const auto commandFlags = static_cast<std::size_t>(channel.muted)
            | (static_cast<std::size_t>(channel.solo) << 1U);
        submitOrWarn({
            navalha::EngineCommandType::setMixerChannel,
            source,
            commandFlags,
            channel.level,
            channel.pan,
            channel.width});
    }

    void syncMixerControls()
    {
        const std::array<const navalha::MixerChannel*, 2> channels {
            &uiMixer.sourceA, &uiMixer.sourceB
        };
        for (std::size_t source = 0; source < channels.size(); ++source)
        {
            mixerLevels[source].setValue(
                channels[source]->level, juce::dontSendNotification);
            mixerPans[source].setValue(
                channels[source]->pan, juce::dontSendNotification);
            mixerWidths[source].setValue(
                channels[source]->width, juce::dontSendNotification);
            mixerMutes[source].setToggleState(
                channels[source]->muted, juce::dontSendNotification);
            mixerSolos[source].setToggleState(
                channels[source]->solo, juce::dontSendNotification);
        }
        mixerBalance.setValue(uiMixer.balance, juce::dontSendNotification);
    }

    [[nodiscard]] std::size_t selectedSliceSource() const
    {
        return static_cast<std::size_t>(std::max(0, sliceSource.getSelectedItemIndex()));
    }

    [[nodiscard]] std::size_t selectedSliceIndex() const
    {
        return static_cast<std::size_t>(std::max(0, sliceIndex.getSelectedItemIndex()));
    }

    void refreshSliceEditor()
    {
        const auto source = selectedSliceSource();
        const auto previous = selectedSliceIndex();
        sliceIndex.clear(juce::dontSendNotification);
        for (std::size_t index = 0; index < uiSliceBanks[source].size(); ++index)
            sliceIndex.addItem(juce::String(index), static_cast<int>(index + 1));
        sliceIndex.setSelectedItemIndex(
            static_cast<int>(std::min(previous, uiSliceBanks[source].size() - 1)),
            juce::dontSendNotification);
        refreshSliceBounds();
        waveform.setSelectedSource(source);
        waveform.setSlices(source, uiSliceBanks[source].slices());
        refreshWaveformEditRange();
    }

    void refreshSliceBounds()
    {
        const auto source = selectedSliceSource();
        const auto index = std::min(
            selectedSliceIndex(), uiSliceBanks[source].size() - 1);
        const auto slice = uiSliceBanks[source].slices()[index];
        sliceStart.setValue(slice.start, juce::dontSendNotification);
        sliceEnd.setValue(slice.end, juce::dontSendNotification);
        if (waveformEditMode == WaveformComponent::EditMode::slice)
            waveform.setEditRange(source, slice.start, slice.end);
    }

    void applySliceBounds()
    {
        const auto source = selectedSliceSource();
        const auto index = selectedSliceIndex();
        const navalha::Slice slice {sliceStart.getValue(), sliceEnd.getValue()};
        if (!slice.isValid())
        {
            showStatus("INVALID SLICE BOUNDS");
            return;
        }
        uiSliceBanks[source].setSlice(index, slice);
        submitOrWarn({
            navalha::EngineCommandType::setSlice,
            source,
            index,
            slice.start,
            slice.end});
        waveform.setSlices(source, uiSliceBanks[source].slices());
        waveform.setEditRange(source, slice.start, slice.end);
    }

    void setWaveformEditMode(WaveformComponent::EditMode mode)
    {
        waveformEditMode = mode;
        waveform.setEditMode(mode);
        selectRegionMode.setToggleState(
            mode == WaveformComponent::EditMode::region,
            juce::dontSendNotification);
        editSliceMode.setToggleState(
            mode == WaveformComponent::EditMode::slice,
            juce::dontSendNotification);
        bladeMode.setToggleState(
            mode == WaveformComponent::EditMode::blade,
            juce::dontSendNotification);
        refreshWaveformEditRange();
    }

    void refreshWaveformEditRange()
    {
        const auto source = selectedSliceSource();
        if (waveformEditMode == WaveformComponent::EditMode::region)
        {
            const auto region = uiSourceRegions[source];
            waveform.setEditRange(source, region.start, region.end);
            return;
        }
        if (waveformEditMode == WaveformComponent::EditMode::slice
            && uiSliceBanks[source].size() != 0)
        {
            const auto index = std::min(
                selectedSliceIndex(), uiSliceBanks[source].size() - 1);
            const auto slice = uiSliceBanks[source].slices()[index];
            waveform.setEditRange(source, slice.start, slice.end);
        }
    }

    void handleWaveformRange(
        std::size_t source, double start, double end)
    {
        if (source >= uiSliceBanks.size())
            return;
        sliceSource.setSelectedItemIndex(
            static_cast<int>(source), juce::dontSendNotification);
        sliceStart.setValue(start, juce::dontSendNotification);
        sliceEnd.setValue(end, juce::dontSendNotification);
        if (waveformEditMode == WaveformComponent::EditMode::region)
        {
            uiSourceRegions[source] = {start, end};
            showStatus(
                (source == 0 ? "SOURCE A" : "SOURCE B")
                + juce::String(" REGION SELECTED | CHOOSE 4/8/16/32/64"));
            return;
        }
        showStatus(
            (source == 0 ? "SOURCE A" : "SOURCE B")
            + juce::String(" SLICE RANGE | PRESS SET TO COMMIT"));
    }

    void selectWholeRegion()
    {
        const auto source = selectedSliceSource();
        uiSourceRegions[source] = {0.0, 1.0};
        sliceStart.setValue(0.0, juce::dontSendNotification);
        sliceEnd.setValue(1.0, juce::dontSendNotification);
        setWaveformEditMode(WaveformComponent::EditMode::region);
        waveform.setEditRange(source, 0.0, 1.0);
        showStatus("WHOLE SOURCE REGION SELECTED");
    }

    void divideSelectedRegion(std::size_t count)
    {
        const auto source = selectedSliceSource();
        const auto region = uiSourceRegions[source];
        if (!region.isValid())
        {
            showStatus("INVALID DIVISION REGION");
            return;
        }
        uiSliceBanks[source].divideRegion(region.start, region.end, count);
        submitOrWarn({
            navalha::EngineCommandType::divideSliceRegion,
            source,
            count,
            region.start,
            region.end});
        refreshSliceEditor();
    }

    void addBladeCut(std::size_t source, double position)
    {
        if (source >= uiSliceBanks.size())
            return;
        sliceSource.setSelectedItemIndex(
            static_cast<int>(source), juce::dontSendNotification);
        if (!uiSliceBanks[source].addBladeCut(position))
        {
            showStatus("BLADE CUT REJECTED");
            return;
        }
        submitOrWarn({
            navalha::EngineCommandType::addBladeCut, source, 0, position});
        refreshSliceEditor();
        showStatus(
            (source == 0 ? "SOURCE A" : "SOURCE B")
            + juce::String(" BLADE CUT | ")
            + juce::String(position, 3));
    }

    void undoBladeCut()
    {
        const auto source = selectedSliceSource();
        if (!uiSliceBanks[source].undoBladeCut())
        {
            showStatus("NO BLADE CUT TO UNDO");
            return;
        }
        submitOrWarn({navalha::EngineCommandType::undoBladeCut, source});
        refreshSliceEditor();
    }

    void showSelectedSourceWaveform()
    {
        const auto source = selectedSliceSource();
        waveform.setSelectedSource(source);
        waveform.setSlices(source, uiSliceBanks[source].slices());
    }

    void submitVoiceProperty(std::size_t voice,
                             navalha::VirtualVoiceProperty property,
                             double value)
    {
        auto& state = uiVirtualVoices[voice];
        switch (property)
        {
            case navalha::VirtualVoiceProperty::enabled:
                state.enabled = value > 0.5;
                break;
            case navalha::VirtualVoiceProperty::source:
                state.sourceIndex = static_cast<std::size_t>(value);
                break;
            case navalha::VirtualVoiceProperty::division:
                state.division = static_cast<std::size_t>(value);
                break;
            case navalha::VirtualVoiceProperty::patternLength:
                state.patternLength = static_cast<std::size_t>(value);
                break;
            case navalha::VirtualVoiceProperty::focusStart:
                state.focusStart = value;
                break;
            case navalha::VirtualVoiceProperty::focusEnd:
                state.focusEnd = value;
                break;
            case navalha::VirtualVoiceProperty::pitch:
                state.pitchSemitones = static_cast<int>(value);
                break;
            case navalha::VirtualVoiceProperty::level:
                state.level = value;
                break;
            case navalha::VirtualVoiceProperty::pan:
                state.pan = value;
                break;
            case navalha::VirtualVoiceProperty::attack:
                state.attackSeconds = value;
                break;
            case navalha::VirtualVoiceProperty::release:
                state.releaseSeconds = value;
                break;
        }
        submitOrWarn({
            navalha::EngineCommandType::setVirtualVoiceProperty,
            voice,
            static_cast<std::size_t>(property),
            value});
    }

    void syncVoiceControls()
    {
        constexpr std::array<std::size_t, 4> divisions {1, 2, 4, 8};
        for (std::size_t voice = 0; voice < uiVirtualVoices.size(); ++voice)
        {
            const auto& state = uiVirtualVoices[voice];
            voiceEnabled[voice].setToggleState(
                state.enabled, juce::dontSendNotification);
            voiceSources[voice].setSelectedItemIndex(
                static_cast<int>(state.sourceIndex), juce::dontSendNotification);
            const auto divisionIterator = std::find(
                divisions.begin(), divisions.end(), state.division);
            voiceDivisions[voice].setSelectedItemIndex(
                static_cast<int>(divisionIterator == divisions.end()
                    ? 0 : std::distance(divisions.begin(), divisionIterator)),
                juce::dontSendNotification);
            voicePitches[voice].setValue(
                state.pitchSemitones, juce::dontSendNotification);
            voiceLevels[voice].setValue(state.level, juce::dontSendNotification);
            voicePans[voice].setValue(state.pan, juce::dontSendNotification);
        }
    }

    [[nodiscard]] std::size_t selectedVoice() const
    {
        return static_cast<std::size_t>(
            std::max(0, voiceEditor.getSelectedItemIndex()));
    }

    void refreshAdvancedVoiceControls()
    {
        const auto& state = uiVirtualVoices[selectedVoice()];
        voicePatternLength.setValue(
            state.patternLength, juce::dontSendNotification);
        voiceFocusStart.setValue(state.focusStart, juce::dontSendNotification);
        voiceFocusEnd.setValue(state.focusEnd, juce::dontSendNotification);
        voiceAttack.setValue(state.attackSeconds, juce::dontSendNotification);
        voiceRelease.setValue(state.releaseSeconds, juce::dontSendNotification);
        refreshVirtualPatternCells();
    }

    void refreshVirtualPatternCells()
    {
        const auto& state = uiVirtualVoices[selectedVoice()];
        for (std::size_t step = 0; step < voicePatternCells.size(); ++step)
        {
            voicePatternCells[step].setValue(
                state.pattern[step], juce::dontSendNotification);
            voicePatternCells[step].setEnabled(step < state.patternLength);
        }
    }

    void stopAudioAndSynchronize()
    {
        engine.suspendOutput();
        shutdownAudio();
        engine.synchronizePendingCommands();
    }

    void syncControlsFromSession()
    {
        tempo.setValue(session.sequencer.tempo(), juce::dontSendNotification);
        division.setSelectedItemIndex(
            static_cast<int>(session.sequencer.division()), juce::dontSendNotification);
        pattern.setSelectedItemIndex(
            static_cast<int>(session.sequencer.currentPattern()),
            juce::dontSendNotification);
        timing.setSelectedItemIndex(
            static_cast<int>(session.sequencer.timing()), juce::dontSendNotification);
        timingSeed = session.sequencer.seed();
        jitterAmount = session.sequencer.jitterPercent();
        jitterControl.setValue(jitterAmount, juce::dontSendNotification);
        timingSeedEditor.setText(
            juce::String(navalha::AssistedRng::formatSeed(timingSeed)), false);
        pitchSemitones.setValue(
            session.heritagePitchSemitones, juce::dontSendNotification);
        pitchMix.setValue(session.heritagePitchMode, juce::dontSendNotification);
        updatePitchModeButtons();
        master.setValue(session.masterLevel, juce::dontSendNotification);
        // Technical output controls are device preferences, not Project state.
        static_cast<void>(engine.setOutputTrimDb(
            static_cast<float>(outputTrim.getValue())));
        engine.setOutputMuted(outputMute.getToggleState());
        uiPatterns = session.patterns;
        uiPatternMemory = session.patternMemory;
        uiPatternTransform = session.patternTransform;
        uiFormDirector.restore(session.formDirector.state());
        uiControlTrace = session.controlTrace;
        uiAssisted = session.assisted;
        uiAssistedSeed = session.assistedRng.seed();
        uiMotifLocks = session.motifLocks;
        refreshPatternCells();
        syncGestureControls();
        syncFormControls();
        syncTraceControls();
        syncAssistedControls();
        syncMotifControls();
        uiMixer = session.mixer;
        syncMixerControls();
        uiSliceBanks = {session.sources[0].sliceBank, session.sources[1].sliceBank};
        for (std::size_t source = 0; source < uiSliceBanks.size(); ++source)
        {
            waveform.setSlices(source, uiSliceBanks[source].slices());
            const auto slices = uiSliceBanks[source].slices();
            if (!slices.empty())
                uiSourceRegions[source] = {
                    slices.front().start, slices.back().end};
        }
        sliceSource.setSelectedItemIndex(
            static_cast<int>(session.activeSource), juce::dontSendNotification);
        refreshSliceEditor();
        uiVirtualVoices = session.virtualVoices;
        syncVoiceControls();
        refreshAdvancedVoiceControls();
    }

    void refreshPatternCells()
    {
        const auto patternIndex =
            static_cast<std::size_t>(std::max(0, pattern.getSelectedItemIndex()));
        for (std::size_t step = 0; step < patternCells.size(); ++step)
            patternCells[step].setValue(
                uiPatterns.cell(patternIndex, step), juce::dontSendNotification);
    }

    void applyPatternMacro(PatternMacro macro)
    {
        const auto patternIndex = selectedPatternIndex();
        const auto countA = std::max<std::size_t>(1, uiSliceBanks[0].size());
        const auto countB = std::max<std::size_t>(1, uiSliceBanks[1].size());
        const auto randomSlice = [this] (std::size_t count)
        {
            return std::min(
                count - 1,
                static_cast<std::size_t>(
                    std::floor(patternMacroRng.next()
                               * static_cast<double>(count))));
        };

        navalha::Pattern row {};
        for (std::size_t step = 0; step < row.size(); ++step)
        {
            switch (macro)
            {
                case PatternMacro::randomA:
                    row[step] = static_cast<std::uint16_t>(randomSlice(countA));
                    break;
                case PatternMacro::randomB:
                    row[step] = static_cast<std::uint16_t>(
                        128 + randomSlice(countB));
                    break;
                case PatternMacro::randomAB:
                {
                    const auto sourceB = patternMacroRng.next() >= 0.5;
                    row[step] = static_cast<std::uint16_t>(
                        (sourceB ? 128 : 0)
                        + randomSlice(sourceB ? countB : countA));
                    break;
                }
                case PatternMacro::interleave:
                    row[step] = static_cast<std::uint16_t>(
                        (step % 2 == 0 ? 0 : 128)
                        + (step / 2) % (step % 2 == 0 ? countA : countB));
                    break;
                case PatternMacro::forward:
                    row[step] = static_cast<std::uint16_t>(step % countA);
                    break;
                case PatternMacro::reverse:
                    row[step] = static_cast<std::uint16_t>(
                        (row.size() - 1 - step) % countA);
                    break;
                case PatternMacro::zero:
                    row[step] = 0;
                    break;
                case PatternMacro::gap:
                    row[step] = navalha::gapCellCode;
                    break;
            }
        }

        uiPatterns.setPattern(patternIndex, row);
        for (std::size_t step = 0; step < row.size(); ++step)
            submitOrWarn({
                navalha::EngineCommandType::setPatternCell,
                patternIndex, step, static_cast<double>(row[step])});
        refreshPatternCells();
        static constexpr std::array<const char*, 8> names {
            "RANDOM A", "RANDOM B", "RANDOM A+B", "INTERLEAVE",
            "0→7", "7→0", "ZERO", "GAP"};
        showStatus("ORDER | " + juce::String::fromUTF8(
            names[static_cast<std::size_t>(macro)]));
    }

    [[nodiscard]] std::size_t selectedPatternIndex() const
    {
        return static_cast<std::size_t>(
            std::max(0, pattern.getSelectedItemIndex()));
    }

    void updateMemoryButton()
    {
        const auto patternIndex = selectedPatternIndex();
        const auto step = static_cast<std::size_t>(
            std::max(0, gestureStep.getSelectedItemIndex()));
        memoryToggle.setButtonText(
            uiPatternMemory[patternIndex][step] ? "MEMORY *" : "MEMORY");
    }

    void syncGestureControls()
    {
        syncingTransformControls = true;
        const auto patternIndex = selectedPatternIndex();
        const auto active = uiPatternTransform.hasBase
            && uiPatternTransform.patternIndex == patternIndex;
        mutationAmount.setValue(
            active ? uiPatternTransform.amounts.mutation : 0,
            juce::dontSendNotification);
        erosionAmount.setValue(
            active ? uiPatternTransform.amounts.erosion : 0,
            juce::dontSendNotification);
        deconstructAmount.setValue(
            active ? uiPatternTransform.amounts.deconstruct : 0,
            juce::dontSendNotification);
        syncingTransformControls = false;
        updateMemoryButton();
    }

    void toggleMemory()
    {
        const auto patternIndex = selectedPatternIndex();
        const auto step = static_cast<std::size_t>(
            std::max(0, gestureStep.getSelectedItemIndex()));
        uiPatternMemory[patternIndex][step] =
            !uiPatternMemory[patternIndex][step];
        submitOrWarn({
            navalha::EngineCommandType::togglePatternMemory,
            patternIndex, step});
        updateMemoryButton();
        showStatus(uiPatternMemory[patternIndex][step]
            ? "STEP PROTECTED IN MEMORY" : "STEP RELEASED FROM MEMORY");
    }

    void applyStructuralTransform()
    {
        if (syncingTransformControls)
            return;

        const auto patternIndex = selectedPatternIndex();
        const navalha::PatternTransformAmounts amounts {
            static_cast<int>(std::lround(mutationAmount.getValue())),
            static_cast<int>(std::lround(erosionAmount.getValue())),
            static_cast<int>(std::lround(deconstructAmount.getValue()))
        };
        if (!uiPatternTransform.hasBase
            || uiPatternTransform.patternIndex != patternIndex)
        {
            uiPatternTransform.hasBase = true;
            uiPatternTransform.patternIndex = patternIndex;
            uiPatternTransform.base = uiPatterns.pattern(patternIndex);
        }
        uiPatternTransform.amounts = amounts;
        uiPatterns.setPattern(
            patternIndex,
            navalha::transformPattern(
                uiPatternTransform.base,
                uiPatternMemory[patternIndex],
                {uiSliceBanks[0].size(), uiSliceBanks[1].size()},
                patternIndex, amounts));
        refreshPatternCells();
        submitOrWarn({
            navalha::EngineCommandType::applyPatternTransform,
            patternIndex, 1,
            static_cast<double>(amounts.mutation),
            static_cast<double>(amounts.erosion),
            static_cast<double>(amounts.deconstruct)});
    }

    void commitTransformState()
    {
        if (!uiPatternTransform.hasBase)
            return;
        submitOrWarn({
            navalha::EngineCommandType::commitPatternTransform,
            uiPatternTransform.patternIndex});
        uiPatternTransform = {};
        syncGestureControls();
        showStatus("STRUCTURAL TRANSFORM COMMITTED");
    }

    void restoreTransformState()
    {
        if (!uiPatternTransform.hasBase)
            return;
        const auto patternIndex = uiPatternTransform.patternIndex;
        uiPatterns.setPattern(patternIndex, uiPatternTransform.base);
        submitOrWarn({
            navalha::EngineCommandType::restorePatternTransform,
            patternIndex});
        uiPatternTransform = {};
        refreshPatternCells();
        syncGestureControls();
        showStatus("STRUCTURAL TRANSFORM RESTORED");
    }

    void startStutter()
    {
        const auto patternIndex = selectedPatternIndex();
        const auto step = static_cast<std::size_t>(
            std::max(0, gestureStep.getSelectedItemIndex()));
        submitOrWarn({
            navalha::EngineCommandType::startStutter,
            0, 0, static_cast<double>(uiPatterns.cell(patternIndex, step))});
        showStatus("STUTTER x4");
    }

    void appendMicroSlices()
    {
        const auto source = selectedSliceSource();
        if (sourceBuffers[source] == nullptr)
        {
            showStatus("LOAD SOURCE BEFORE MICRO");
            return;
        }
        const auto duration = static_cast<double>(sourceBuffers[source]->size())
            / sourceBuffers[source]->sampleRate();
        const auto slice = selectedSliceIndex();
        const auto added = uiSliceBanks[source].appendMicroSlices(
            slice, 8, duration);
        if (added == 0)
        {
            showStatus("MICRO SLICES REJECTED");
            return;
        }
        submitOrWarn({
            navalha::EngineCommandType::appendMicroSlices,
            source, slice, 8.0, duration});
        refreshSliceEditor();
        showStatus("MICRO SLICES x" + juce::String(added));
    }

    void syncFormControls()
    {
        syncingFormControls = true;
        const auto& form = uiFormDirector.state();
        formScene.clear(juce::dontSendNotification);
        for (std::size_t index = 0; index < form.sceneCount; ++index)
            formScene.addItem(
                juce::String::fromUTF8(
                    navalha::formText(form.scenes[index].name).data(),
                    static_cast<int>(
                        navalha::formText(form.scenes[index].name).size())),
                static_cast<int>(index + 1));
        formScene.setSelectedItemIndex(
            static_cast<int>(form.currentScene), juce::dontSendNotification);
        const auto& scene = form.scenes[form.currentScene];
        formBars.setValue(scene.bars, juce::dontSendNotification);
        formEnergy.setValue(scene.energy, juce::dontSendNotification);
        formVariation.setValue(scene.variation, juce::dontSendNotification);
        formTransition.setSelectedItemIndex(
            static_cast<int>(scene.transition), juce::dontSendNotification);
        formBankA.setSelectedItemIndex(
            static_cast<int>(scene.bankA), juce::dontSendNotification);
        formBankB.setSelectedItemIndex(
            static_cast<int>(scene.bankB), juce::dontSendNotification);
        formDensity.setValue(scene.density, juce::dontSendNotification);
        formTension.setValue(scene.tension, juce::dontSendNotification);
        formStability.setValue(scene.stability, juce::dontSendNotification);
        formContinuity.setValue(scene.continuity, juce::dontSendNotification);
        formContrast.setValue(scene.contrast, juce::dontSendNotification);
        formStereoMotion.setValue(
            scene.stereoMotion, juce::dontSendNotification);
        formLock.setButtonText(scene.locked ? "UNLOCK" : "LOCK");
        formEnable.setButtonText(form.enabled ? "FORM ON" : "ARM FORM");
        formHold.setButtonText(form.hold ? "RELEASE" : "HOLD");
        formHold.setEnabled(form.enabled);
        syncingFormControls = false;
    }

    void applyUiFormSceneMaterial()
    {
        const auto& form = uiFormDirector.state();
        const auto& scene = form.scenes[form.currentScene];
        const std::array profiles {scene.bankA, scene.bankB};
        for (std::size_t source = 0; source < uiSliceBanks.size(); ++source)
        {
            const auto count = navalha::generatedSliceCount(profiles[source]);
            if (count == 0)
                continue;
            const auto slices = uiSliceBanks[source].slices();
            uiSliceBanks[source].divideRegion(
                slices.empty() ? 0.0 : slices.front().start,
                slices.empty() ? 1.0 : slices.back().end,
                count);
        }
        refreshSliceEditor();
    }

    void editFormScene()
    {
        if (syncingFormControls)
            return;
        auto scene = uiFormDirector.state().scenes[
            uiFormDirector.state().currentScene];
        scene.bars = static_cast<int>(std::lround(formBars.getValue()));
        scene.energy = static_cast<int>(std::lround(formEnergy.getValue()));
        scene.variation = static_cast<int>(
            std::lround(formVariation.getValue()));
        if (!uiFormDirector.replaceCurrentScene(scene))
            showStatus("FORM SCENE LOCKED");
        else
            submitOrWarn({
                navalha::EngineCommandType::setFormSceneBasic,
                uiFormDirector.state().currentScene, 0,
                static_cast<double>(scene.bars),
                static_cast<double>(scene.energy),
                static_cast<double>(scene.variation)});
        syncFormControls();
    }

    void editFormProfiles()
    {
        if (syncingFormControls)
            return;
        auto scene = uiFormDirector.state().scenes[
            uiFormDirector.state().currentScene];
        scene.transition = static_cast<navalha::FormTransition>(
            std::max(0, formTransition.getSelectedItemIndex()));
        scene.bankA = static_cast<navalha::SliceBankProfile>(
            std::max(0, formBankA.getSelectedItemIndex()));
        scene.bankB = static_cast<navalha::SliceBankProfile>(
            std::max(0, formBankB.getSelectedItemIndex()));
        scene.density = static_cast<int>(std::lround(formDensity.getValue()));
        scene.tension = static_cast<int>(std::lround(formTension.getValue()));
        scene.stability = static_cast<int>(std::lround(formStability.getValue()));
        if (!uiFormDirector.replaceCurrentScene(scene))
        {
            showStatus("FORM SCENE LOCKED");
            syncFormControls();
            return;
        }
        const auto packed =
            static_cast<std::size_t>(scene.transition)
            | (static_cast<std::size_t>(scene.bankA) << 8U)
            | (static_cast<std::size_t>(scene.bankB) << 16U);
        submitOrWarn({
            navalha::EngineCommandType::setFormSceneProfiles,
            uiFormDirector.state().currentScene, packed,
            static_cast<double>(scene.density),
            static_cast<double>(scene.tension),
            static_cast<double>(scene.stability)});
        applyUiFormSceneMaterial();
    }

    void editFormCharacter()
    {
        if (syncingFormControls)
            return;
        auto scene = uiFormDirector.state().scenes[
            uiFormDirector.state().currentScene];
        scene.continuity =
            static_cast<int>(std::lround(formContinuity.getValue()));
        scene.contrast =
            static_cast<int>(std::lround(formContrast.getValue()));
        scene.stereoMotion =
            static_cast<int>(std::lround(formStereoMotion.getValue()));
        if (!uiFormDirector.replaceCurrentScene(scene))
        {
            showStatus("FORM SCENE LOCKED");
            syncFormControls();
            return;
        }
        submitOrWarn({
            navalha::EngineCommandType::setFormSceneCharacter,
            uiFormDirector.state().currentScene, 0,
            static_cast<double>(scene.continuity),
            static_cast<double>(scene.contrast),
            static_cast<double>(scene.stereoMotion)});
    }

    void moveFormScene(int delta)
    {
        if (!uiFormDirector.moveScene(delta))
            return;
        submitOrWarn({
            navalha::EngineCommandType::moveFormScene,
            0, 0, static_cast<double>(delta)});
        syncFormControls();
    }

    void syncAssistedControls()
    {
        syncingAssistedControls = true;
        assistedEnable.setToggleState(
            uiAssisted.enabled, juce::dontSendNotification);
        assistedRepeat.setToggleState(
            uiAssisted.repeat, juce::dontSendNotification);
        assistedSource.setToggleState(
            uiAssisted.chooseSource, juce::dontSendNotification);
        assistedOrder.setToggleState(
            uiAssisted.changeOrder, juce::dontSendNotification);
        assistedRegion.setToggleState(
            uiAssisted.editRegion, juce::dontSendNotification);
        assistedCuts.setToggleState(
            uiAssisted.editSlices, juce::dontSendNotification);
        assistedMix.setToggleState(
            uiAssisted.autoMix, juce::dontSendNotification);
        assistedTransform.setToggleState(
            uiAssisted.applyTransform, juce::dontSendNotification);
        assistedGaps.setToggleState(
            uiAssisted.useGaps, juce::dontSendNotification);
        assistedPitch.setToggleState(
            uiAssisted.changePitch, juce::dontSendNotification);
        assistedFragments.setToggleState(
            uiAssisted.useFragments, juce::dontSendNotification);
        assistedMinBpm.setValue(
            uiAssisted.minBpm, juce::dontSendNotification);
        assistedMaxBpm.setValue(
            uiAssisted.maxBpm, juce::dontSendNotification);
        assistedVariation.setValue(
            uiAssisted.variation, juce::dontSendNotification);
        assistedSeed.setText(
            navalha::AssistedRng::formatSeed(uiAssistedSeed), false);
        syncMotifLocks();
        syncingAssistedControls = false;
    }

    void submitAssistedSettings()
    {
        if (syncingAssistedControls)
            return;
        uiAssisted.enabled = assistedEnable.getToggleState();
        uiAssisted.repeat = assistedRepeat.getToggleState();
        uiAssisted.chooseSource = assistedSource.getToggleState();
        uiAssisted.changeOrder = assistedOrder.getToggleState();
        uiAssisted.editRegion = assistedRegion.getToggleState();
        uiAssisted.editSlices = assistedCuts.getToggleState();
        uiAssisted.autoMix = assistedMix.getToggleState();
        uiAssisted.applyTransform = assistedTransform.getToggleState();
        uiAssisted.useGaps = assistedGaps.getToggleState();
        uiAssisted.changePitch = assistedPitch.getToggleState();
        uiAssisted.useFragments = assistedFragments.getToggleState();
        uiAssisted.minBpm =
            static_cast<int>(std::lround(assistedMinBpm.getValue()));
        uiAssisted.maxBpm =
            static_cast<int>(std::lround(assistedMaxBpm.getValue()));
        uiAssisted.variation =
            static_cast<int>(std::lround(assistedVariation.getValue()));
        navalha::normalizeAssistedSettings(uiAssisted);
        const auto assistedFlags =
            static_cast<std::size_t>(uiAssisted.enabled)
            | (static_cast<std::size_t>(uiAssisted.chooseSource) << 1U)
            | (static_cast<std::size_t>(uiAssisted.changeOrder) << 2U)
            | (static_cast<std::size_t>(uiAssisted.editRegion) << 3U)
            | (static_cast<std::size_t>(uiAssisted.editSlices) << 4U)
            | (static_cast<std::size_t>(uiAssisted.autoMix) << 5U)
            | (static_cast<std::size_t>(uiAssisted.applyTransform) << 6U)
            | (static_cast<std::size_t>(uiAssisted.useGaps) << 7U)
            | (static_cast<std::size_t>(uiAssisted.changePitch) << 8U)
            | (static_cast<std::size_t>(uiAssisted.useFragments) << 9U)
            | (static_cast<std::size_t>(uiAssisted.repeat) << 10U);
        submitOrWarn({
            navalha::EngineCommandType::setAssistedSettings,
            assistedFlags, 0,
            static_cast<double>(uiAssisted.minBpm),
            static_cast<double>(uiAssisted.maxBpm),
            static_cast<double>(uiAssisted.variation)});
        syncAssistedControls();
    }

    void syncMotifLocks()
    {
        lockSource.setToggleState(
            uiMotifLocks.source, juce::dontSendNotification);
        lockCuts.setToggleState(
            uiMotifLocks.cuts, juce::dontSendNotification);
        lockPattern.setToggleState(
            uiMotifLocks.pattern, juce::dontSendNotification);
        lockTransform.setToggleState(
            uiMotifLocks.transform, juce::dontSendNotification);
        lockPitch.setToggleState(
            uiMotifLocks.pitch, juce::dontSendNotification);
        lockGap.setToggleState(
            uiMotifLocks.gap, juce::dontSendNotification);
        lockMix.setToggleState(
            uiMotifLocks.mix, juce::dontSendNotification);
        lockVoices.setToggleState(
            uiMotifLocks.voices, juce::dontSendNotification);
    }

    void submitMotifLocks()
    {
        uiMotifLocks = {
            lockSource.getToggleState(),
            lockCuts.getToggleState(),
            lockPattern.getToggleState(),
            lockTransform.getToggleState(),
            lockPitch.getToggleState(),
            lockGap.getToggleState(),
            lockMix.getToggleState(),
            lockVoices.getToggleState()};
        const auto lockFlags =
            static_cast<std::size_t>(uiMotifLocks.source)
            | (static_cast<std::size_t>(uiMotifLocks.cuts) << 1U)
            | (static_cast<std::size_t>(uiMotifLocks.pattern) << 2U)
            | (static_cast<std::size_t>(uiMotifLocks.transform) << 3U)
            | (static_cast<std::size_t>(uiMotifLocks.pitch) << 4U)
            | (static_cast<std::size_t>(uiMotifLocks.gap) << 5U)
            | (static_cast<std::size_t>(uiMotifLocks.mix) << 6U)
            | (static_cast<std::size_t>(uiMotifLocks.voices) << 7U);
        submitOrWarn({
            navalha::EngineCommandType::setMotifLocks, lockFlags});
    }

    [[nodiscard]] juce::String defaultMotifName(std::size_t slot) const
    {
        return "MOTIF " + juce::String(static_cast<int>(slot + 1))
            .paddedLeft('0', 2);
    }

    void syncMotifControls()
    {
        syncingMotifControls = true;
        for (std::size_t slot = 0; slot < motifSlotButtons.size(); ++slot)
        {
            motifSlotButtons[slot].setButtonText(
                juce::String(static_cast<int>(slot + 1))
                + (uiMotifSlots[slot].occupied ? "*" : ""));
            motifSlotButtons[slot].setToggleState(
                slot == selectedMotifSlot, juce::dontSendNotification);
        }
        const auto& snapshot = uiMotifSlots[selectedMotifSlot];
        motifName.setText(
            snapshot.occupied && !snapshot.name.empty()
                ? juce::String(snapshot.name)
                : defaultMotifName(selectedMotifSlot),
            false);
        motifRecall.setEnabled(snapshot.occupied);
        motifVary.setEnabled(snapshot.occupied);
        motifDelete.setEnabled(snapshot.occupied);
        motifInfo.setText(
            snapshot.occupied
                ? "STORED | PATTERN "
                    + juce::String(static_cast<int>(
                        snapshot.currentPattern + 1)).paddedLeft('0', 2)
                : "EMPTY SLOT",
            juce::dontSendNotification);
        syncingMotifControls = false;
    }

    void captureMotif()
    {
        auto& snapshot = uiMotifSlots[selectedMotifSlot];
        snapshot.occupied = true;
        const auto typedName = motifName.getText().trim().substring(0, 28);
        snapshot.name = (
            typedName.isEmpty()
                ? defaultMotifName(selectedMotifSlot) : typedName).toStdString();
        snapshot.capturedAt =
            juce::Time::getCurrentTime().toISO8601(true).toStdString();
        for (std::size_t source = 0; source < snapshot.sources.size(); ++source)
        {
            snapshot.sources[source].sliceBank = uiSliceBanks[source];
            snapshot.sources[source].hasAudio =
                sourceBuffers[source] != nullptr;
        }
        snapshot.currentPattern = selectedPatternIndex();
        snapshot.pattern = uiPatterns.pattern(snapshot.currentPattern);
        snapshot.cellMemory = uiPatternMemory[snapshot.currentPattern];
        snapshot.activeSource = selectedSliceSource();
        snapshot.bpm = tempo.getValue();
        snapshot.divisionMode = static_cast<std::size_t>(
            std::max(0, division.getSelectedItemIndex()));
        snapshot.timingMode = static_cast<navalha::TimingMode>(
            std::max(0, timing.getSelectedItemIndex()));
        snapshot.jitter = jitterAmount;
        snapshot.heritagePitchSemitones =
            static_cast<int>(std::lround(pitchSemitones.getValue()));
        snapshot.heritagePitchMode = pitchMix.getValue();
        snapshot.mixer = uiMixer;
        snapshot.virtualVoices = uiVirtualVoices;
        syncMotifControls();
        showStatus(
            "MOTIF " + juce::String(static_cast<int>(selectedMotifSlot + 1))
            + " CAPTURED | " + juce::String(snapshot.name));
    }

    void submitMotifSliceBank(
        std::size_t source, const navalha::SliceBank& bank)
    {
        const auto slices = bank.slices();
        if (slices.empty())
            return;
        submitOrWarn({
            navalha::EngineCommandType::divideSliceRegion,
            source,
            slices.size(),
            slices.front().start,
            slices.back().end});
        for (std::size_t index = 0; index < slices.size(); ++index)
            submitOrWarn({
                navalha::EngineCommandType::setSlice,
                source,
                index,
                slices[index].start,
                slices[index].end});
    }

    void submitMotifVoices()
    {
        for (std::size_t voice = 0; voice < uiVirtualVoices.size(); ++voice)
        {
            const auto& state = uiVirtualVoices[voice];
            const std::array<std::pair<navalha::VirtualVoiceProperty, double>, 11>
                voiceProperties {{
                    {navalha::VirtualVoiceProperty::enabled,
                     state.enabled ? 1.0 : 0.0},
                    {navalha::VirtualVoiceProperty::source,
                     static_cast<double>(state.sourceIndex)},
                    {navalha::VirtualVoiceProperty::division,
                     static_cast<double>(state.division)},
                    {navalha::VirtualVoiceProperty::patternLength,
                     static_cast<double>(state.patternLength)},
                    {navalha::VirtualVoiceProperty::focusStart, state.focusStart},
                    {navalha::VirtualVoiceProperty::focusEnd, state.focusEnd},
                    {navalha::VirtualVoiceProperty::pitch,
                     static_cast<double>(state.pitchSemitones)},
                    {navalha::VirtualVoiceProperty::level, state.level},
                    {navalha::VirtualVoiceProperty::pan, state.pan},
                    {navalha::VirtualVoiceProperty::attack,
                     state.attackSeconds},
                    {navalha::VirtualVoiceProperty::release,
                     state.releaseSeconds}
                }};
            for (const auto& [property, value] : voiceProperties)
                submitOrWarn({
                    navalha::EngineCommandType::setVirtualVoiceProperty,
                    voice, static_cast<std::size_t>(property), value});
            for (std::size_t step = 0; step < state.patternLength; ++step)
                submitOrWarn({
                    navalha::EngineCommandType::setVirtualVoicePatternCell,
                    voice, step, static_cast<double>(state.pattern[step])});
        }
    }

    void recallMotif(bool vary)
    {
        const auto& snapshot = uiMotifSlots[selectedMotifSlot];
        if (!snapshot.occupied)
            return;
        if (uiPatternTransform.hasBase)
            commitTransformState();

        const auto previousMemory =
            uiPatternMemory[snapshot.currentPattern];
        for (std::size_t source = 0; source < uiSliceBanks.size(); ++source)
        {
            uiSliceBanks[source] = snapshot.sources[source].sliceBank;
            const auto slices = uiSliceBanks[source].slices();
            if (!slices.empty())
                uiSourceRegions[source] = {
                    slices.front().start, slices.back().end};
            submitMotifSliceBank(source, uiSliceBanks[source]);
            waveform.setSlices(source, uiSliceBanks[source].slices());
        }

        pattern.setSelectedItemIndex(
            static_cast<int>(snapshot.currentPattern),
            juce::dontSendNotification);
        uiPatterns.setPattern(snapshot.currentPattern, snapshot.pattern);
        for (std::size_t step = 0; step < navalha::stepsPerPattern; ++step)
        {
            submitOrWarn({
                navalha::EngineCommandType::setPatternCell,
                snapshot.currentPattern, step,
                static_cast<double>(snapshot.pattern[step])});
            if (previousMemory[step] != snapshot.cellMemory[step])
                submitOrWarn({
                    navalha::EngineCommandType::togglePatternMemory,
                    snapshot.currentPattern, step});
        }
        uiPatternMemory[snapshot.currentPattern] = snapshot.cellMemory;
        submitOrWarn({
            navalha::EngineCommandType::selectPattern,
            snapshot.currentPattern});

        sliceSource.setSelectedItemIndex(
            static_cast<int>(snapshot.activeSource),
            juce::dontSendNotification);
        submitOrWarn({
            navalha::EngineCommandType::selectSource,
            snapshot.activeSource});
        tempo.setValue(snapshot.bpm, juce::dontSendNotification);
        division.setSelectedItemIndex(
            static_cast<int>(snapshot.divisionMode),
            juce::dontSendNotification);
        submitOrWarn({
            navalha::EngineCommandType::setTempo,
            snapshot.divisionMode, 0, snapshot.bpm});
        timing.setSelectedItemIndex(
            static_cast<int>(snapshot.timingMode),
            juce::dontSendNotification);
        jitterAmount = snapshot.jitter;
        jitterControl.setValue(jitterAmount, juce::dontSendNotification);
        submitTiming();
        pitchSemitones.setValue(
            snapshot.heritagePitchSemitones, juce::dontSendNotification);
        pitchMix.setValue(
            snapshot.heritagePitchMode, juce::dontSendNotification);
        submitPitch();

        uiMixer = snapshot.mixer;
        syncMixerControls();
        submitMixer(0);
        submitMixer(1);
        submitOrWarn({
            navalha::EngineCommandType::setMixerBalance,
            0, 0, uiMixer.balance});
        uiVirtualVoices = snapshot.virtualVoices;
        syncVoiceControls();
        refreshAdvancedVoiceControls();
        submitMotifVoices();

        refreshPatternCells();
        syncGestureControls();
        refreshSliceEditor();
        if (vary)
            varyRecalledMotif();
        showStatus(
            "MOTIF " + juce::String(static_cast<int>(selectedMotifSlot + 1))
            + (vary ? " VARIED | " : " RECALLED | ")
            + juce::String(snapshot.name));
    }

    void varyRecalledMotif()
    {
        const auto patternIndex = selectedPatternIndex();
        auto row = uiPatterns.pattern(patternIndex);
        if (!uiMotifLocks.pattern)
        {
            std::array<std::size_t, navalha::stepsPerPattern> editable {};
            std::size_t editableCount = 0;
            for (std::size_t step = 0; step < row.size(); ++step)
                if (!uiPatternMemory[patternIndex][step])
                    editable[editableCount++] = step;
            const auto changes = std::min<std::size_t>(
                editableCount,
                1 + static_cast<std::size_t>(
                    motifVariationRng.next() * 3.0));
            for (std::size_t change = 0; change < changes; ++change)
            {
                const auto choice = change + static_cast<std::size_t>(
                    motifVariationRng.next()
                    * static_cast<double>(editableCount - change));
                std::swap(editable[change], editable[
                    std::min(choice, editableCount - 1)]);
                const auto step = editable[change];
                const auto keepGap =
                    uiMotifLocks.gap && row[step] == navalha::gapCellCode;
                if (keepGap)
                    continue;
                const auto source = uiMotifLocks.source
                    ? selectedSliceSource()
                    : static_cast<std::size_t>(
                        motifVariationRng.next() >= 0.5);
                const auto count =
                    std::max<std::size_t>(1, uiSliceBanks[source].size());
                const auto slice = static_cast<std::uint16_t>(
                    motifVariationRng.next() * static_cast<double>(count));
                row[step] = static_cast<std::uint16_t>(
                    source == 0 ? slice : 128U + slice);
            }
            uiPatterns.setPattern(patternIndex, row);
            for (std::size_t step = 0; step < row.size(); ++step)
                submitOrWarn({
                    navalha::EngineCommandType::setPatternCell,
                    patternIndex, step, static_cast<double>(row[step])});
            refreshPatternCells();
        }
        if (!uiMotifLocks.pitch && motifVariationRng.next() < 0.55)
        {
            constexpr std::array<int, 6> offsets {-5, -3, -2, 2, 3, 5};
            const auto offset = offsets[std::min<std::size_t>(
                offsets.size() - 1,
                static_cast<std::size_t>(
                    motifVariationRng.next() * offsets.size()))];
            pitchSemitones.setValue(
                std::clamp(
                    static_cast<int>(std::lround(pitchSemitones.getValue()))
                        + offset,
                    -12, 11),
                juce::sendNotification);
            pitchMix.setValue(1.0, juce::sendNotification);
        }
        if (!uiMotifLocks.mix && motifVariationRng.next() < 0.65)
        {
            uiMixer.balance = std::clamp(
                uiMixer.balance + (motifVariationRng.next() - 0.5) * 0.34,
                -1.0, 1.0);
            for (auto* channel : {&uiMixer.sourceA, &uiMixer.sourceB})
            {
                channel->pan = std::clamp(
                    channel->pan + (motifVariationRng.next() - 0.5) * 0.28,
                    -1.0, 1.0);
                channel->width = std::clamp(
                    channel->width + (motifVariationRng.next() - 0.5) * 0.30,
                    0.0, 2.0);
            }
            syncMixerControls();
            submitMixer(0);
            submitMixer(1);
            submitOrWarn({
                navalha::EngineCommandType::setMixerBalance,
                0, 0, uiMixer.balance});
        }
    }

    void deleteMotif()
    {
        uiMotifSlots[selectedMotifSlot] = {};
        syncMotifControls();
        showStatus(
            "MOTIF " + juce::String(static_cast<int>(selectedMotifSlot + 1))
            + " DELETED");
    }

    void chooseAudioLibraryDirectory()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Choose audio library folder",
            audioLibrary.rootDirectory(), "*");
        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& chooser)
            {
                const auto directory = chooser.getResult();
                if (directory.isDirectory())
                {
                    audioLibrary.setRootDirectory(directory);
                    librarySearch.clear();
                    libraryPath.setText(
                        directory.getFileName().isEmpty()
                            ? directory.getFullPathName()
                            : directory.getFileName(),
                        juce::dontSendNotification);
                    libraryPath.setTooltip(directory.getFullPathName());
                    refreshLibraryHint();
                    showStatus(
                        "OPEN FILES | " + directory.getFullPathName());
                }
                fileChooser.reset();
            });
    }

    void chooseSource(std::size_t sourceIndex)
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            sourceIndex == 0 ? "Load SOURCE A" : "Load SOURCE B",
            juce::File {}, "*.wav;*.wave;*.aif;*.aiff");
        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [this, sourceIndex] (const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();
                if (file.existsAsFile())
                    loadSource(sourceIndex, file);
                fileChooser.reset();
            });
    }

    void loadSource(std::size_t sourceIndex, const juce::File& file)
    {
        try
        {
            auto decoded = decodeSourceFile(file);
            auto newPeaks =
                navalha::buildWaveformPeaks(*decoded.audio, 2048);

            recorder.stop();
            stopAudioAndSynchronize();
            engine.setSourceBuffer(sourceIndex, nullptr);
            sourceBuffers[sourceIndex] = std::move(decoded.audio);
            sourceWavData[sourceIndex] = std::move(decoded.portableWav);
            sourceMediaTypes[sourceIndex] = decoded.mediaType;
            sourceFiles[sourceIndex] = file;
            const auto sizeInMb =
                static_cast<double>(file.getSize()) / (1024.0 * 1024.0);
            selectedInfo.setText(
                file.getFileName() + "\n"
                    + file.getFileExtension().trimCharactersAtStart(".").toUpperCase()
                    + " | " + juce::String(sizeInMb, 1)
                    + juce::String::fromUTF8(" MB · SOURCE ")
                    + (sourceIndex == 0 ? "A" : "B") + " READY",
                juce::dontSendNotification);
            selectedInfo.setTooltip(file.getFullPathName());
            engine.setSourceBuffer(sourceIndex, sourceBuffers[sourceIndex].get());
            session.selectSource(sourceIndex);
            uiSliceBanks[sourceIndex] = session.sources[sourceIndex].sliceBank;
            uiSourceRegions[sourceIndex] = {0.0, 1.0};
            sliceSource.setSelectedItemIndex(
                static_cast<int>(sourceIndex), juce::dontSendNotification);
            waveform.setPeaks(sourceIndex, std::move(newPeaks));
            refreshSliceEditor();
            setAudioChannels(0, 2);
            showStatus((sourceIndex == 0 ? "SOURCE A | " : "SOURCE B | ")
                       + file.getFileName());
        }
        catch (const std::exception& exception)
        {
            showStatus("INVALID AUDIO | " + juce::String(exception.what()));
        }
    }

    [[nodiscard]] navalha::ProjectStateV2 captureProjectWithReferences(
        const juce::File& projectFile) const
    {
        auto project = navalha::captureProjectState(session);
        project.motifSlots = uiMotifSlots;
        project.selectedMotifSlot = selectedMotifSlot;
        const auto projectDirectory = projectFile.getParentDirectory();
        for (std::size_t source = 0; source < sourceFiles.size(); ++source)
        {
            const auto& audioFile = sourceFiles[source];
            if (!audioFile.existsAsFile())
                continue;
            auto& reference = project.sourceReferences[source];
            reference.filename = audioFile.getFileName().toStdString();
            if (audioFile.getParentDirectory() == projectDirectory)
                reference.relativePath = reference.filename;
            reference.size = static_cast<std::uint64_t>(audioFile.getSize());
            reference.lastModified = static_cast<std::uint64_t>(std::max(
                juce::int64 {0},
                audioFile.getLastModificationTime().toMilliseconds()));
            reference.mediaType = sourceMediaTypes[source];
        }
        return project;
    }

    [[nodiscard]] std::size_t loadLightweightProjectSources(
        const juce::File& projectFile,
        const navalha::ProjectStateV2& project)
    {
        constexpr std::int64_t maximumSourceBytes = 512LL * 1024LL * 1024LL;
        const auto directory = projectFile.getParentDirectory();
        std::size_t missing = 0;
        for (std::size_t source = 0; source < sourceBuffers.size(); ++source)
        {
            engine.setSourceBuffer(source, nullptr);
            sourceBuffers[source].reset();
            sourceWavData[source].clear();
            sourceFiles[source] = juce::File {};
            sourceMediaTypes[source] = "audio/wav";

            const auto& reference = project.sourceReferences[source];
            if (reference.filename.empty() && reference.relativePath.empty())
                continue;

            const auto relative = reference.relativePath.empty()
                ? reference.filename : reference.relativePath;
            juce::File audioFile;
            if (navalha::isSafePortableRelativePath(relative))
            {
                audioFile = directory.getChildFile(juce::String(relative));
                if (!audioFile.isAChildOf(directory))
                    audioFile = juce::File {};
            }
            if (!audioFile.existsAsFile()
                || audioFile.getSize() <= 0
                || audioFile.getSize() > maximumSourceBytes
                || (reference.size != 0
                    && static_cast<std::uint64_t>(audioFile.getSize()) != reference.size))
            {
                ++missing;
                continue;
            }

            try
            {
                auto decoded = decodeSourceFile(audioFile);
                sourceBuffers[source] = std::move(decoded.audio);
                sourceWavData[source] = std::move(decoded.portableWav);
                sourceMediaTypes[source] = decoded.mediaType;
                sourceFiles[source] = audioFile;
            }
            catch (const std::exception&)
            {
                sourceBuffers[source].reset();
                sourceWavData[source].clear();
                sourceMediaTypes[source] = "audio/wav";
                ++missing;
            }
        }
        return missing;
    }

    void chooseProjectToSave()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Save Navalha Project", juce::File {}, "*.navalha");
        fileChooser->launchAsync(
            juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (file != juce::File {})
                {
                    if (!file.hasFileExtension("navalha"))
                        file = file.withFileExtension("navalha");
                    stopAudioAndSynchronize();
                    try
                    {
                        const auto json = navalha::encodeProjectJson(
                            captureProjectWithReferences(file));
                        const auto saved = file.replaceWithText(json);
                        setAudioChannels(0, 2);
                        showStatus(saved ? "PROJECT SAVED | " + file.getFileName()
                                         : "PROJECT SAVE FAILED");
                    }
                    catch (const std::exception& exception)
                    {
                        setAudioChannels(0, 2);
                        showStatus("PROJECT SAVE FAILED | "
                                   + juce::String(exception.what()));
                    }
                }
                fileChooser.reset();
            });
    }

    void choosePortableToSave()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Save Portable Navalha Project", juce::File {}, "*.zip");
        fileChooser->launchAsync(
            juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (file != juce::File {})
                {
                    if (!file.hasFileExtension("zip"))
                        file = file.withFileExtension("zip");
                    stopAudioAndSynchronize();
                    try
                    {
                        const auto archive = navalha::createPortableProject(
                            captureProjectWithReferences(file),
                            sourceWavData[0],
                            sourceWavData[1]);
                        const auto saved = file.replaceWithData(
                            archive.data(), archive.size());
                        setAudioChannels(0, 2);
                        showStatus(saved ? "PORTABLE SAVED | " + file.getFileName()
                                         : "PORTABLE SAVE FAILED");
                    }
                    catch (const std::exception& exception)
                    {
                        setAudioChannels(0, 2);
                        showStatus("PORTABLE SAVE FAILED | "
                                   + juce::String(exception.what()));
                    }
                }
                fileChooser.reset();
            });
    }

    void chooseProjectToOpen()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Open Navalha Project", juce::File {}, "*.navalha;*.zip");
        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();
                if (file.existsAsFile())
                    openProjectFile(file);
                fileChooser.reset();
            });
    }

    void openProjectFile(const juce::File& file)
    {
        juce::MemoryBlock bytes;
        if (!file.loadFileAsData(bytes))
        {
            showStatus("PROJECT READ FAILED");
            return;
        }

        recorder.stop();
        stopAudioAndSynchronize();
        try
        {
            std::size_t missingSources = 0;
            navalha::ProjectStateV2 openedProject;
            if (file.hasFileExtension("zip"))
            {
                auto loaded = navalha::openPortableProject({
                    static_cast<const std::uint8_t*>(bytes.getData()), bytes.getSize()});
                openedProject = loaded.project;
                navalha::restoreProjectState(loaded.project, session);
                sourceBuffers[0] = std::move(loaded.sourceA);
                sourceBuffers[1] = std::move(loaded.sourceB);
                sourceWavData[0] = std::move(loaded.sourceAWav);
                sourceWavData[1] = std::move(loaded.sourceBWav);
                sourceFiles = {};
                sourceMediaTypes = {"audio/wav", "audio/wav"};
            }
            else
            {
                const std::string json(
                    static_cast<const char*>(bytes.getData()), bytes.getSize());
                openedProject = navalha::decodeProjectJson(json);
                navalha::restoreProjectState(openedProject, session);
                missingSources = loadLightweightProjectSources(
                    file, openedProject);
            }
            uiMotifSlots = openedProject.motifSlots;
            selectedMotifSlot = openedProject.selectedMotifSlot;

            for (std::size_t source = 0; source < sourceBuffers.size(); ++source)
                engine.setSourceBuffer(source, sourceBuffers[source].get());
            for (std::size_t source = 0;
                 source < sourceBuffers.size(); ++source)
                waveform.setPeaks(
                    source,
                    sourceBuffers[source] == nullptr
                        ? std::vector<navalha::WaveformPeak> {}
                        : navalha::buildWaveformPeaks(
                            *sourceBuffers[source], 2048));
            syncControlsFromSession();
            setAudioChannels(0, 2);
            showStatus(
                "PROJECT OPENED | " + file.getFileName()
                + (missingSources == 0
                    ? juce::String()
                    : " | " + juce::String(missingSources) + " SOURCE MISSING"));
        }
        catch (const std::exception& exception)
        {
            setAudioChannels(0, 2);
            showStatus("INVALID PROJECT | " + juce::String(exception.what()));
        }
    }

    [[nodiscard]] std::string captureTakeRecipe() const
    {
        navalha::ProjectStateV2 snapshot;
        for (std::size_t source = 0; source < snapshot.sources.size(); ++source)
        {
            snapshot.sources[source].sliceBank = uiSliceBanks[source];
            snapshot.sources[source].hasAudio = sourceBuffers[source] != nullptr;
            const auto& file = sourceFiles[source];
            if (file.existsAsFile())
            {
                auto& reference = snapshot.sourceReferences[source];
                reference.filename = file.getFileName().toStdString();
                reference.size = static_cast<std::uint64_t>(file.getSize());
                reference.lastModified = static_cast<std::uint64_t>(std::max(
                    juce::int64 {0},
                    file.getLastModificationTime().toMilliseconds()));
                reference.mediaType = sourceMediaTypes[source];
            }
        }
        snapshot.patterns = uiPatterns;
        snapshot.mixer = uiMixer;
        snapshot.virtualVoices = uiVirtualVoices;
        snapshot.patternMemory = uiPatternMemory;
        snapshot.patternTransform = uiPatternTransform;
        snapshot.formDirector = uiFormDirector.state();
        snapshot.controlTrace = uiControlTrace;
        snapshot.activeSource = selectedSliceSource();
        snapshot.currentPattern = static_cast<std::size_t>(
            std::max(0, pattern.getSelectedItemIndex()));
        snapshot.bpm = tempo.getValue();
        snapshot.divisionMode = static_cast<std::size_t>(
            std::max(0, division.getSelectedItemIndex()));
        snapshot.timingMode = static_cast<navalha::TimingMode>(
            std::clamp(timing.getSelectedItemIndex(), 0, 2));
        snapshot.jitter = jitterAmount;
        snapshot.timingSeed = timingSeed;
        snapshot.heritagePitchSemitones = static_cast<int>(
            std::lround(pitchSemitones.getValue()));
        snapshot.heritagePitchMode = pitchMix.getValue();
        snapshot.masterLevel = master.getValue();
        snapshot.assistedSeed = uiAssistedSeed;
        snapshot.assistedState = uiAssistedSeed;
        snapshot.assistedCursor = 0;
        snapshot.assisted = uiAssisted;
        snapshot.motifLocks = uiMotifLocks;
        snapshot.motifSlots = uiMotifSlots;
        snapshot.selectedMotifSlot = selectedMotifSlot;

        const auto project = navalha::parseJson(
            navalha::encodeProjectJson(snapshot));
        return navalha::serializeJson(navalha::Json::Object {
            {"format", "navalha-take-recipe"},
            {"version", 1},
            {"appVersion", JUCE_APPLICATION_VERSION_STRING},
            {"capturedAt", juce::Time::getCurrentTime().toISO8601(true).toStdString()},
            {"captureScope", "ui-state-before-recording"},
            {"assistedCursorAvailable", false},
            {"project", project}
        }) + "\n";
    }

    [[nodiscard]] static std::string makeTakeId(
        const juce::File& file, const juce::Time& created)
    {
        return ("take-"
                + juce::String::toHexString(created.toMilliseconds()) + "-"
                + file.getFileNameWithoutExtension()
                      .retainCharacters(
                          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")
                      .substring(0, 80)).toStdString();
    }

    void registerFinalizedTake()
    {
        if (!activeRecordingFile.existsAsFile()
            || activeRecordingRate == 0
            || activeRecordingCreatedAt.toMilliseconds() <= 0)
            return;
        navalha::TakeEntry entry;
        entry.id = makeTakeId(activeRecordingFile, activeRecordingCreatedAt);
        entry.audioPath = activeRecordingFile.getFullPathName().toStdString();
        entry.filename = activeRecordingFile.getFileName().toStdString();
        entry.createdAt = activeRecordingCreatedAt.toISO8601(true).toStdString();
        entry.frames = recorder.framesWritten();
        entry.sampleRate = activeRecordingRate;
        entry.durationSeconds = static_cast<double>(entry.frames)
            / static_cast<double>(entry.sampleRate);
        entry.sampleFormat = activeRecordingFormat;
        entry.metadata = activeRecordingMetadata;
        entry.recipeJson = activeRecordingRecipe;
        takeCatalog.upsert(std::move(entry));
        saveTakeCatalog();
    }

    void clearActiveRecordingRegistration()
    {
        activeRecordingFile = juce::File {};
        activeRecordingCreatedAt = {};
        activeRecordingRate = 0;
        activeRecordingRecipe.clear();
        activeRecordingMetadata = {};
    }

    void chooseRecordingPath()
    {
        if (recorder.isRunning())
        {
            showStatus("RECORDING ALREADY ACTIVE");
            return;
        }
        fileChooser = std::make_unique<juce::FileChooser>(
            "Record MASTER WAV", juce::File {}, "*.wav");
        fileChooser->launchAsync(
            juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (file != juce::File {})
                {
                    if (!file.hasFileExtension("wav"))
                        file = file.withFileExtension("wav");
                    const auto rate = static_cast<std::uint32_t>(
                        activeSampleRate.load(std::memory_order_acquire));
                    const auto formatIndex =
                        std::max(0, recordingFormat.getSelectedItemIndex());
                    const auto format = formatIndex == 0
                        ? navalha::WavSampleFormat::pcm16
                        : formatIndex == 2
                            ? navalha::WavSampleFormat::float32
                            : navalha::WavSampleFormat::pcm24;
                    const auto bytesPerFrame =
                        format == navalha::WavSampleFormat::pcm16 ? 4ULL
                        : format == navalha::WavSampleFormat::pcm24 ? 6ULL : 8ULL;
                    constexpr std::uint64_t diskReserve =
                        1024ULL * 1024ULL * 1024ULL;
                    const auto freeBytes = static_cast<std::uint64_t>(std::max(
                        juce::int64 {0},
                        file.getParentDirectory().getBytesFreeOnVolume()));
                    if (freeBytes <= diskReserve)
                    {
                        showStatus("RECORDING REFUSED | LESS THAN 1 GiB FREE");
                        fileChooser.reset();
                        return;
                    }
                    const auto spaceFrames =
                        (freeBytes - diskReserve) / bytesPerFrame;
                    constexpr auto maxRecordingSeconds = 5ULL * 60ULL;
                    const auto fiveMinuteFrames =
                        static_cast<std::uint64_t>(rate) * maxRecordingSeconds;
                    const auto riffFrames =
                        std::numeric_limits<std::uint32_t>::max() / bytesPerFrame;
                    const auto maximumFrames =
                        std::min({spaceFrames, fiveMinuteFrames, riffFrames});
                    if (maximumFrames < static_cast<std::uint64_t>(rate) * 10ULL)
                    {
                        showStatus("RECORDING REFUSED | INSUFFICIENT SAFE SPACE");
                        fileChooser.reset();
                        return;
                    }
                    const navalha::WavMetadata metadata {
                        "Navalha 2 recording", "Navalha 2",
                        "JUCE migration", "", ""};
                    std::string recipe;
                    try
                    {
                        recipe = captureTakeRecipe();
                    }
                    catch (const std::exception& exception)
                    {
                        showStatus("RECORDING RECIPE FAILED | "
                                   + juce::String(exception.what()));
                        fileChooser.reset();
                        return;
                    }
                    const auto createdAt = juce::Time::getCurrentTime();
                    const auto started = recorder.start(
                        file.getFullPathName().toStdString(),
                        rate,
                        format,
                        metadata,
                        maximumFrames);
                    recorderObservedRunning = started;
                    if (started)
                    {
                        activeRecordingFile = file;
                        activeRecordingCreatedAt = createdAt;
                        activeRecordingRate = rate;
                        activeRecordingFormat = format;
                        activeRecordingMetadata = metadata;
                        activeRecordingRecipe = std::move(recipe);
                    }
                    else
                    {
                        clearActiveRecordingRegistration();
                    }
                    showStatus(started ? "RECORDING | " + file.getFileName()
                                       : "RECORDING FAILED | "
                                           + juce::String(recorder.error()));
                }
                fileChooser.reset();
            });
    }

    void showStatus(const juce::String& message)
    {
        status.setText(message, juce::dontSendNotification);
        juce::StringArray lines;
        lines.addLines(activityLog.getText());
        lines.add(message);
        while (lines.size() > 4)
            lines.remove(0);
        activityLog.setText(lines.joinIntoString("\n"), false);
    }

    navalha::SessionModel session;
    navalha::AudioEngine engine;
    navalha::RecordingWriterService recorder {engine};
    navalha::TakeCatalog takeCatalog;
    juce::File activeRecordingFile;
    juce::Time activeRecordingCreatedAt;
    std::uint32_t activeRecordingRate = 0;
    navalha::WavSampleFormat activeRecordingFormat =
        navalha::WavSampleFormat::pcm24;
    navalha::WavMetadata activeRecordingMetadata;
    std::string activeRecordingRecipe;
    std::atomic<double> activeSampleRate {44100.0};
    std::array<std::unique_ptr<navalha::StereoAudioBuffer>, 2> sourceBuffers;
    std::array<std::vector<std::uint8_t>, 2> sourceWavData;
    std::array<juce::File, 2> sourceFiles;
    std::array<std::string, 2> sourceMediaTypes {
        "audio/wav", "audio/wav"};
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Label title;
    juce::Label status;
    juce::TextButton audioConnectionStatus;
    juce::Label libraryLabel;
    juce::Label logLabel;
    juce::Label selectedLabel;
    juce::Label selectedInfo;
    juce::Label transportClock;
    juce::Label libraryHint;
    juce::Label libraryPath;
    juce::TextEditor librarySearch;
    juce::TextEditor activityLog;
    juce::Label learnModeLabel;
    juce::Label learnTitle;
    juce::TextEditor learnBody;
    juce::Label masterLabel;
    juce::Label outputTrimLabel;
    juce::Label tempoLabel;
    juce::Label divisionLabel;
    juce::Label patternLabel;
    juce::Label timingLabel;
    juce::Label jitterLabel;
    juce::Label timingSeedLabel;
    juce::Label transportInfo;
    juce::Label pitchLabel;
    juce::Label pitchMixLabel;
    juce::Label patternCellsLabel;
    juce::Label orderLabel;
    juce::Label gestureLabel;
    juce::Label formLabel;
    juce::Label traceLabel;
    juce::Label traceInfo;
    juce::Label assistedLabel;
    juce::Label motifLabel;
    juce::Label mixerHeaderLabel;
    juce::Label mixerLevelLabel;
    juce::Label mixerPanLabel;
    juce::Label mixerWidthLabel;
    juce::Label mixerBalanceLabel;
    std::array<juce::Label, 2> mixerSourceLabels;
    juce::Label sliceEditorLabel;
    juce::Label waveEditLabel;
    juce::Label divideRegionLabel;
    juce::Label voicesHeaderLabel;
    std::array<juce::Label, 2> voiceLabels;
    juce::Label voiceAdvancedLabel;
    juce::Label voicePatternLabel;
    juce::Label outputMeterLabel;
    juce::Label recordingInfo;
    juce::Label recordingFormatLabel;
    juce::TextButton play;
    juce::TextButton stop;
    juce::TextButton resetTransport;
    juce::TextButton openProject;
    juce::TextButton saveProject;
    juce::TextButton savePortable;
    juce::TextButton record;
    juce::ToggleButton outputMute;
    juce::TextButton pitchBypass;
    juce::TextButton pitchZero;
    juce::TextButton pitchAudition;
    juce::TextButton copyLog;
    juce::TextButton clearLog;
    juce::TextButton chooseLibraryFolder;
    juce::TextButton loadSelectedA;
    juce::TextButton loadSelectedB;
    juce::TextButton previewSelected;
    juce::TextButton stopPreview;
    juce::Slider master {juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider outputTrim {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider tempo {juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::ComboBox division;
    juce::ComboBox pattern;
    juce::ComboBox timing;
    juce::Slider jitterControl {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::TextEditor timingSeedEditor;
    juce::TextButton applyTimingSeed;
    juce::Slider pitchSemitones {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider pitchMix {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    std::array<juce::Slider, navalha::stepsPerPattern> patternCells;
    juce::TextButton randomA;
    juce::TextButton randomB;
    juce::TextButton randomAB;
    juce::TextButton interleave;
    juce::TextButton forwardOrder;
    juce::TextButton reverseOrder;
    juce::TextButton zeroOrder;
    juce::TextButton gapOrder;
    navalha::AssistedRng patternMacroRng {0x4f524445U};
    navalha::PatternBank uiPatterns;
    juce::ComboBox gestureStep;
    juce::TextButton memoryToggle;
    juce::Slider mutationAmount {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider erosionAmount {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider deconstructAmount {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::TextButton commitTransform;
    juce::TextButton restoreTransform;
    juce::TextButton stutter;
    juce::TextButton burst;
    juce::TextButton micro;
    juce::TextButton reverseSlice;
    std::array<navalha::PatternMemory, navalha::patternCount> uiPatternMemory {};
    navalha::PatternTransformState uiPatternTransform;
    bool syncingTransformControls = false;
    juce::ComboBox formScene;
    juce::TextButton formEnable;
    juce::TextButton formHold;
    juce::TextButton formNext;
    juce::TextButton formReset;
    juce::Slider formBars {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider formEnergy {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider formVariation {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::ComboBox formTransition;
    juce::ComboBox formBankA;
    juce::ComboBox formBankB;
    juce::Slider formDensity {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider formTension {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider formStability {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider formContinuity {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider formContrast {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider formStereoMotion {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::TextButton formLock;
    juce::TextButton formAdd;
    juce::TextButton formDuplicate;
    juce::TextButton formDelete;
    juce::TextButton formMoveUp;
    juce::TextButton formMoveDown;
    bool syncingFormControls = false;
    navalha::FormDirector uiFormDirector;
    juce::TextButton traceRecord;
    juce::TextButton traceLoop;
    juce::TextButton traceClear;
    ControlTracePad tracePad;
    navalha::ControlTrace uiControlTrace;
    bool traceArmed = false;
    bool traceRecording = false;
    bool traceLooping = false;
    double traceStartedAt = 0.0;
    juce::ToggleButton assistedEnable;
    juce::ToggleButton assistedRepeat;
    juce::ToggleButton assistedSource;
    juce::ToggleButton assistedOrder;
    juce::ToggleButton assistedRegion;
    juce::ToggleButton assistedCuts;
    juce::ToggleButton assistedMix;
    juce::ToggleButton assistedTransform;
    juce::ToggleButton assistedGaps;
    juce::ToggleButton assistedPitch;
    juce::ToggleButton assistedFragments;
    juce::Slider assistedMinBpm {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider assistedMaxBpm {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider assistedVariation {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::TextEditor assistedSeed;
    juce::TextButton assistedApplySeed;
    juce::TextButton assistedRewind;
    juce::TextButton assistedNext;
    juce::TextButton assistedKeep;
    juce::TextButton assistedRestore;
    std::array<juce::TextButton, navalha::motifSlotCount> motifSlotButtons;
    juce::TextEditor motifName;
    juce::TextButton motifCapture;
    juce::TextButton motifRecall;
    juce::TextButton motifVary;
    juce::TextButton motifDelete;
    juce::Label motifInfo;
    std::array<navalha::MotifSnapshot, navalha::motifSlotCount> uiMotifSlots;
    std::size_t selectedMotifSlot = 0;
    navalha::AssistedRng motifVariationRng {0x4d4f5446U};
    bool syncingMotifControls = false;
    juce::ToggleButton lockSource;
    juce::ToggleButton lockCuts;
    juce::ToggleButton lockPattern;
    juce::ToggleButton lockTransform;
    juce::ToggleButton lockPitch;
    juce::ToggleButton lockGap;
    juce::ToggleButton lockMix;
    juce::ToggleButton lockVoices;
    navalha::AssistedPerformerSettings uiAssisted;
    navalha::MotifLocks uiMotifLocks;
    std::uint32_t uiAssistedSeed = navalha::AssistedRng::defaultSeed;
    bool syncingAssistedControls = false;
    std::array<juce::Slider, 2> mixerLevels;
    std::array<juce::Slider, 2> mixerPans;
    std::array<juce::Slider, 2> mixerWidths;
    std::array<juce::ToggleButton, 2> mixerMutes;
    std::array<juce::ToggleButton, 2> mixerSolos;
    navalha::SourceMixer uiMixer;
    juce::Slider mixerBalance {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::ComboBox sliceSource;
    juce::ComboBox sliceIndex;
    juce::Slider sliceStart;
    juce::Slider sliceEnd;
    juce::TextButton setSlice;
    juce::TextButton selectRegionMode;
    juce::TextButton editSliceMode;
    juce::TextButton bladeMode;
    std::array<juce::TextButton, 5> divideRegionButtons;
    juce::TextButton undoBlade;
    juce::TextButton wholeRegion;
    juce::TextButton playSlice;
    std::array<navalha::SliceBank, 2> uiSliceBanks;
    std::array<navalha::Slice, 2> uiSourceRegions {
        navalha::Slice {0.0, 1.0}, navalha::Slice {0.0, 1.0}};
    WaveformComponent::EditMode waveformEditMode =
        WaveformComponent::EditMode::region;
    std::array<juce::ToggleButton, 2> voiceEnabled;
    std::array<juce::ComboBox, 2> voiceSources;
    std::array<juce::ComboBox, 2> voiceDivisions;
    std::array<juce::Slider, 2> voicePitches;
    std::array<juce::Slider, 2> voiceLevels;
    std::array<juce::Slider, 2> voicePans;
    std::array<navalha::VirtualVoiceState, 2> uiVirtualVoices;
    juce::ComboBox voiceEditor;
    juce::Slider voicePatternLength;
    juce::Slider voiceFocusStart;
    juce::Slider voiceFocusEnd;
    juce::Slider voiceAttack;
    juce::Slider voiceRelease;
    std::array<juce::Slider, 16> voicePatternCells;
    double meterLeft = 0.0;
    double meterRight = 0.0;
    int safetyHoldTicks = 0;
    double heldSafetyGainReductionDb = 0.0;
    juce::ProgressBar outputLeftMeter {meterLeft};
    juce::ProgressBar outputRightMeter {meterRight};
    juce::ComboBox recordingFormat;
    juce::ApplicationProperties applicationProperties;
    navalha::ui::Language uiLanguage = navalha::ui::Language::english;
    juce::String activeLearnKey;
    bool learningMode = false;
    bool dualMonitorLayout = false;
    juce::Rectangle<int> visibleViewportArea;
    bool recorderObservedRunning = false;
    bool displayedTransportRunning = false;
    std::size_t displayedTransportStep = navalha::stepsPerPattern;
    double transportStartedAtMilliseconds = 0.0;
    double transportElapsedMilliseconds = 0.0;
    std::size_t displayedFormScene = 0;
    int displayedFormBar = 0;
    bool displayedFormCompleted = false;
    std::uint32_t timingSeed = 0x4e415632U;
    double jitterAmount = 18.0;
    WaveformComponent waveform;
    std::unique_ptr<juce::Drawable> arcadeLogo;
    AudioLibraryList audioLibrary;
    juce::File selectedLibraryFile;
    juce::AudioFormatManager previewFormatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> previewReader;
    juce::AudioTransportSource previewTransport;
    juce::AudioBuffer<float> previewScratch {2, 8192};
};

class TakeTimelineComponent final : public juce::Component,
                                    private juce::ListBoxModel,
                                    private juce::Timer
{
public:
    TakeTimelineComponent(
        MainComponent& owner,
        std::function<void(const juce::File&)> sendAction = {})
        : main(owner), sendToMasterAction(std::move(sendAction))
    {
        setLookAndFeel(&lookAndFeel);
        heading.setText("TAKE TIMELINE", juce::dontSendNotification);
        heading.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 18.0F, juce::Font::bold)));
        heading.getProperties().set("arcadeFontSize", 18.0);
        heading.getProperties().set("arcadeFontBold", true);
        heading.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        addAndMakeVisible(heading);

        summary.setJustificationType(juce::Justification::centredRight);
        summary.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        addAndMakeVisible(summary);
        configureButton(importFolder, "IMPORT WAV FOLDER", [this]
        {
            chooseImportDirectory();
        });

        list.setModel(this);
        list.setRowHeight(58);
        list.setColour(
            juce::ListBox::backgroundColourId, juce::Colour(Arcade::background));
        list.setColour(
            juce::ListBox::outlineColourId, juce::Colour(Arcade::line));
        list.setOutlineThickness(1);
        addAndMakeVisible(list);

        configureLabel(selectedLabel, "SELECTED TAKE");
        selectedName.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        selectedName.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 13.0F, juce::Font::bold)));
        selectedName.getProperties().set("arcadeFontSize", 13.0);
        selectedName.getProperties().set("arcadeFontBold", true);
        addAndMakeVisible(selectedName);
        selectedInfo.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        addAndMakeVisible(selectedInfo);

        configureLabel(titleLabel, "TITLE");
        configureLabel(artistLabel, "ARTIST");
        configureLabel(projectLabel, "PROJECT / ALBUM");
        configureLabel(yearLabel, "YEAR");
        configureLabel(commentLabel, "COMMENT");
        configureLabel(statusLabel, "STATUS");
        configureLabel(ratingLabel, "RATING");
        configureLabel(tagsLabel, "TAGS");
        configureLabel(notesLabel, "REVIEW NOTES");
        for (auto* editor : {
                 &title, &artist, &project, &year, &comment, &tags, &notes})
        {
            styleEditableTextField(*editor);
            addAndMakeVisible(*editor);
        }
        status.addItemList({
            "EXPERIMENT", "CANDIDATE", "SELECTED",
            "APPROVED", "REJECTED", "MASTER"}, 1);
        rating.addItemList(
            {juce::String::fromUTF8("—"), "1", "2", "3", "4", "5"}, 1);
        addAndMakeVisible(status);
        addAndMakeVisible(rating);

        configureButton(useA, "USE AS SOURCE A", [this]
        {
            if (!selectedId.empty())
                static_cast<void>(main.useTakeAsSource(selectedId, 0));
        });
        configureButton(useB, "USE AS SOURCE B", [this]
        {
            if (!selectedId.empty())
                static_cast<void>(main.useTakeAsSource(selectedId, 1));
        });
        configureButton(save, "SAVE METADATA / REVIEW", [this] { saveSelected(); });
        configureButton(exportRecipe, "EXPORT RECIPE JSON", [this]
        {
            exportSelectedRecipe();
        });
        configureButton(sendToMaster, "SEND TO MASTER", [this]
        {
            const auto* entry = main.take(selectedId);
            if (entry == nullptr || !sendToMasterAction)
                return;
            const juce::File file(utf8(entry->audioPath));
            if (file.existsAsFile())
                sendToMasterAction(file);
        });
        useA.getProperties().set("arcadeAccent", "play");
        useB.getProperties().set("arcadeAccent", "play");
        save.getProperties().set("arcadeAccent", "record");
        sendToMaster.getProperties().set("arcadeAccent", "play");
        sendToMaster.getProperties().set("learnKey", "masterwindow");

        note.setText(
            "The recorded WAV remains unchanged. Review data and the performance "
            "recipe are stored in Navalha's private TAKE catalog.",
            juce::dontSendNotification);
        note.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        note.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(note);

        // LEARN is displayed in the fixed panel of the main window.  Giving
        // this detached workspace a key also covers list rows and editor
        // internals that JUCE creates as child components.
        getProperties().set("learnKey", "takes");

        setSize(1120, 740);
        setEditorEnabled(false);
        refreshCatalog();
        startTimerHz(2);
    }

    ~TakeTimelineComponent() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
        list.setModel(nullptr);
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(tintedPanelSurface(
            juce::Colour(Arcade::red), 0.10F));
        graphics.setColour(
            juce::Colour(Arcade::line).interpolatedWith(
                juce::Colour(Arcade::red), 0.55F));
        graphics.drawRect(getLocalBounds().toFloat().reduced(0.5F));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16);
        auto header = area.removeFromTop(42);
        heading.setBounds(header.removeFromLeft(260));
        importFolder.setBounds(header.removeFromLeft(170).reduced(2));
        summary.setBounds(header);
        area.removeFromTop(8);
        auto left = area.removeFromLeft(420);
        area.removeFromLeft(14);
        list.setBounds(left);

        auto selectedRow = area.removeFromTop(28);
        selectedLabel.setBounds(selectedRow.removeFromLeft(130));
        selectedName.setBounds(selectedRow);
        selectedInfo.setBounds(area.removeFromTop(24));
        area.removeFromTop(8);

        layoutEditorRow(area, titleLabel, title);
        layoutEditorRow(area, artistLabel, artist);
        layoutEditorRow(area, projectLabel, project);
        layoutEditorRow(area, yearLabel, year);
        layoutEditorRow(area, commentLabel, comment);
        auto reviewRow = area.removeFromTop(42);
        statusLabel.setBounds(reviewRow.removeFromLeft(70).reduced(2));
        status.setBounds(reviewRow.removeFromLeft(150).reduced(2));
        ratingLabel.setBounds(reviewRow.removeFromLeft(70).reduced(2));
        rating.setBounds(reviewRow.removeFromLeft(90).reduced(2));
        layoutEditorRow(area, tagsLabel, tags);
        layoutEditorRow(area, notesLabel, notes);
        area.removeFromTop(8);
        auto sourceActions = area.removeFromTop(42);
        useA.setBounds(sourceActions.removeFromLeft(
            sourceActions.getWidth() / 2).reduced(2));
        useB.setBounds(sourceActions.reduced(2));
        auto saveActions = area.removeFromTop(42);
        const auto saveActionWidth = saveActions.getWidth() / 3;
        save.setBounds(
            saveActions.removeFromLeft(saveActionWidth).reduced(2));
        exportRecipe.setBounds(
            saveActions.removeFromLeft(saveActionWidth).reduced(2));
        sendToMaster.setBounds(saveActions.reduced(2));
        note.setBounds(area.removeFromTop(48).reduced(4));
    }

private:
    void timerCallback() override
    {
        refreshCatalog();
    }

    int getNumRows() override
    {
        return static_cast<int>(main.takes().size());
    }

    const navalha::TakeEntry* entryForRow(int row) const noexcept
    {
        const auto& entries = main.takes();
        if (!juce::isPositiveAndBelow(row, static_cast<int>(entries.size())))
            return nullptr;
        return &entries[entries.size() - 1 - static_cast<std::size_t>(row)];
    }

    void paintListBoxItem(int row,
                          juce::Graphics& graphics,
                          int width,
                          int height,
                          bool selected) override
    {
        const auto* entry = entryForRow(row);
        if (entry == nullptr)
            return;
        graphics.fillAll(selected
            ? juce::Colour(0xff20251f) : juce::Colour(Arcade::surface));
        graphics.setColour(juce::Colour(Arcade::line));
        graphics.drawHorizontalLine(height - 1, 5.0F, width - 5.0F);
        auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(8, 4);
        graphics.setColour(selected
            ? juce::Colour(Arcade::yellowHigh) : juce::Colour(Arcade::ink));
        graphics.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 11.0F, juce::Font::bold)));
        graphics.drawFittedText(
            utf8(entry->filename), bounds.removeFromTop(23),
            juce::Justification::centredLeft, 1);
        const auto stars = entry->review.rating == 0
            ? juce::String() : " | " + juce::String::repeatedString(
                  "*", entry->review.rating);
        const auto availability = juce::File(utf8(entry->audioPath))
                .existsAsFile()
            ? juce::String() : " | AUDIO MISSING";
        graphics.setColour(juce::Colour(Arcade::muted));
        graphics.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 9.0F, juce::Font::plain)));
        graphics.drawFittedText(
            utf8(entry->review.status) + stars + " | "
                + juce::String(entry->durationSeconds, 2) + " s"
                + availability,
            bounds, juce::Justification::centredLeft, 1);
    }

    void selectedRowsChanged(int row) override
    {
        const auto* entry = entryForRow(row);
        if (entry == nullptr)
        {
            selectedId.clear();
            setEditorEnabled(false);
            return;
        }
        selectedId = entry->id;
        selectedName.setText(utf8(entry->filename), juce::dontSendNotification);
        selectedInfo.setText(
            utf8(entry->createdAt) + " | "
                + juce::String(entry->sampleRate) + " Hz | "
                + navalha::toString(entry->sampleFormat) + " | "
                + juce::String(entry->frames) + " FRAMES",
            juce::dontSendNotification);
        title.setText(utf8(entry->metadata.title), false);
        artist.setText(utf8(entry->metadata.artist), false);
        project.setText(utf8(entry->metadata.project), false);
        year.setText(utf8(entry->metadata.year), false);
        comment.setText(utf8(entry->metadata.comment), false);
        tags.setText(utf8(entry->review.tags), false);
        notes.setText(utf8(entry->review.notes), false);
        status.setText(utf8(entry->review.status), juce::dontSendNotification);
        rating.setSelectedItemIndex(
            std::clamp(entry->review.rating, 0, 5),
            juce::dontSendNotification);
        setEditorEnabled(true);
        exportRecipe.setEnabled(!entry->recipeJson.empty());
        sendToMaster.setEnabled(
            static_cast<bool>(sendToMasterAction)
            && juce::File(utf8(entry->audioPath)).existsAsFile());
    }

    void refreshCatalog()
    {
        const auto count = main.takes().size();
        if (count != displayedCount)
        {
            displayedCount = count;
            list.updateContent();
            if (count != 0 && selectedId.empty())
                list.selectRow(0);
        }
        list.repaint();
        summary.setText(
            juce::String(count) + (count == 1 ? " TAKE" : " TAKES"),
            juce::dontSendNotification);
    }

    void saveSelected()
    {
        const auto* current = main.take(selectedId);
        if (current == nullptr)
            return;
        auto entry = *current;
        entry.metadata = {
            title.getText().toStdString(), artist.getText().toStdString(),
            project.getText().toStdString(), year.getText().toStdString(),
            comment.getText().toStdString()};
        entry.review = {
            status.getText().toStdString(),
            std::max(0, rating.getSelectedItemIndex()),
            tags.getText().toStdString(), notes.getText().toStdString()};
        if (main.updateTake(std::move(entry)))
        {
            list.repaint();
            selectedRowsChanged(list.getSelectedRow());
        }
    }

    void exportSelectedRecipe()
    {
        const auto* entry = main.take(selectedId);
        if (entry == nullptr || entry->recipeJson.empty())
            return;
        auto suggested = juce::File(utf8(entry->filename))
            .withFileExtension("recipe.json");
        chooser = std::make_unique<juce::FileChooser>(
            "Export TAKE recipe", suggested, "*.json");
        const auto id = selectedId;
        chooser->launchAsync(
            juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, id] (const juce::FileChooser& selected)
            {
                auto file = selected.getResult();
                const auto* current = main.take(id);
                if (file != juce::File {} && current != nullptr)
                {
                    if (!file.hasFileExtension("json"))
                        file = file.withFileExtension("json");
                    static_cast<void>(file.replaceWithText(current->recipeJson));
                }
                chooser.reset();
            });
    }

    void chooseImportDirectory()
    {
        chooser = std::make_unique<juce::FileChooser>(
            "Import previous Navalha WAV recordings",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& selected)
            {
                const auto directory = selected.getResult();
                if (directory.isDirectory())
                {
                    static_cast<void>(main.importTakeDirectory(directory));
                    refreshCatalog();
                }
                chooser.reset();
            });
    }

    void setEditorEnabled(bool enabled)
    {
        const std::array<juce::Component*, 14> components {
            &title, &artist, &project, &year, &comment, &status, &rating,
            &tags, &notes, &useA, &useB, &save, &exportRecipe,
            &sendToMaster};
        for (auto* component : components)
            component->setEnabled(enabled);
    }

    void configureLabel(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        label.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 10.0F, juce::Font::bold)));
        label.getProperties().set("arcadeFontSize", 10.0);
        label.getProperties().set("arcadeFontBold", true);
        addAndMakeVisible(label);
    }

    void configureButton(juce::TextButton& button,
                         const juce::String& text,
                         std::function<void()> action)
    {
        button.setButtonText(text);
        button.onClick = std::move(action);
        addAndMakeVisible(button);
    }

    static void layoutEditorRow(juce::Rectangle<int>& area,
                                juce::Label& label,
                                juce::TextEditor& editor)
    {
        auto row = area.removeFromTop(42);
        label.setBounds(row.removeFromLeft(120).reduced(2));
        editor.setBounds(row.reduced(2));
    }

    MainComponent& main;
    juce::Label heading;
    juce::Label summary;
    juce::ListBox list;
    juce::Label selectedLabel;
    juce::Label selectedName;
    juce::Label selectedInfo;
    juce::Label titleLabel;
    juce::Label artistLabel;
    juce::Label projectLabel;
    juce::Label yearLabel;
    juce::Label commentLabel;
    juce::Label statusLabel;
    juce::Label ratingLabel;
    juce::Label tagsLabel;
    juce::Label notesLabel;
    juce::TextEditor title;
    juce::TextEditor artist;
    juce::TextEditor project;
    juce::TextEditor year;
    juce::TextEditor comment;
    juce::ComboBox status;
    juce::ComboBox rating;
    juce::TextEditor tags;
    juce::TextEditor notes;
    ArcadeLookAndFeel lookAndFeel;
    juce::TextButton useA;
    juce::TextButton useB;
    juce::TextButton save;
    juce::TextButton exportRecipe;
    juce::TextButton sendToMaster;
    juce::TextButton importFolder;
    juce::Label note;
    std::unique_ptr<juce::FileChooser> chooser;
    std::function<void(const juce::File&)> sendToMasterAction;
    std::string selectedId;
    std::size_t displayedCount = std::numeric_limits<std::size_t>::max();
};

class PerformanceXYPad final : public juce::Component
{
public:
    std::function<void(double, int)> onMove;

    void setValues(double newBpm, int newPitch)
    {
        bpm = std::clamp(newBpm, 20.0, 400.0);
        pitch = std::clamp(newPitch, -12, 11);
        repaint();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(Arcade::surface));
        graphics.setColour(juce::Colour(Arcade::line));
        for (int division = 1; division < 4; ++division)
        {
            const auto x = getWidth() * division / 4;
            const auto y = getHeight() * division / 4;
            graphics.drawVerticalLine(
                x, 0.0F, static_cast<float>(getHeight()));
            graphics.drawHorizontalLine(
                y, 0.0F, static_cast<float>(getWidth()));
        }
        const auto x = static_cast<float>(
            (bpm - 20.0) / 380.0 * static_cast<double>(getWidth()));
        const auto y = static_cast<float>(
            (11.0 - static_cast<double>(pitch)) / 23.0
            * static_cast<double>(getHeight()));
        graphics.setColour(juce::Colour(Arcade::yellow).withAlpha(0.28F));
        graphics.drawVerticalLine(
            static_cast<int>(x), 0.0F, static_cast<float>(getHeight()));
        graphics.drawHorizontalLine(
            static_cast<int>(y), 0.0F, static_cast<float>(getWidth()));
        graphics.setColour(juce::Colour(Arcade::yellowHigh));
        graphics.fillEllipse(x - 8.0F, y - 8.0F, 16.0F, 16.0F);
        graphics.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 12.0F, juce::Font::bold)));
        graphics.drawFittedText(
            "XY MOD | BPM " + juce::String(std::lround(bpm))
                + " | PITCH " + juce::String(pitch),
            getLocalBounds().reduced(10), juce::Justification::topLeft, 1);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        moveTo(event.position);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        moveTo(event.position);
    }

private:
    void moveTo(juce::Point<float> point)
    {
        const auto normalizedX = std::clamp(
            static_cast<double>(point.x) / std::max(1, getWidth()), 0.0, 1.0);
        const auto normalizedY = std::clamp(
            static_cast<double>(point.y) / std::max(1, getHeight()), 0.0, 1.0);
        bpm = 20.0 + normalizedX * 380.0;
        pitch = std::clamp(
            static_cast<int>(std::lround(11.0 - normalizedY * 23.0)),
            -12, 11);
        repaint();
        if (onMove)
            onMove(bpm, pitch);
    }

    double bpm = 120.0;
    int pitch = 0;
};

class PerformanceRemoteComponent final : public juce::Component,
                                         private juce::Timer
{
public:
    explicit PerformanceRemoteComponent(MainComponent& owner)
        : main(owner)
    {
        setLookAndFeel(&lookAndFeel);
        title.setText(
            "DETACHED PERFORM | ONE ENGINE",
            juce::dontSendNotification);
        title.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        title.setJustificationType(juce::Justification::centred);
        title.getProperties().set("arcadeTitle", true);
        addAndMakeVisible(title);

        configure(play, "PLAY", [this] { main.remotePlay(); });
        play.getProperties().set("arcadeAccent", "play");
        configure(stop, "STOP", [this] { main.remoteStop(); });
        stop.getProperties().set("arcadeAccent", "stop");
        configure(reset, "RESET", [this] { main.remoteReset(); });
        configure(next, "NEXT", [this] { main.remoteNextAssisted(); });
        configure(record, "REC", [this] { main.remoteToggleRecording(); });
        record.getProperties().set("arcadeAccent", "record");
        configure(sourceA, "SOURCE A", [this] { main.remoteSetSource(0); });
        configure(sourceB, "SOURCE B", [this] { main.remoteSetSource(1); });
        sourceA.setClickingTogglesState(true);
        sourceB.setClickingTogglesState(true);
        sourceA.setTooltip("Use SOURCE A for the performance.");
        sourceB.setTooltip("Use SOURCE B for the performance.");
        sourceA.getProperties().set("arcadeAccent", "sourceA");
        sourceB.getProperties().set("arcadeAccent", "sourceB");

        tempo.setRange(20.0, 400.0, 1.0);
        tempo.setTextValueSuffix(" BPM");
        tempo.onValueChange = [this]
        {
            if (!syncing)
                main.remoteSetTempo(tempo.getValue());
        };
        addAndMakeVisible(tempo);
        pattern.addItemList(
            {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10"}, 1);
        pattern.onChange = [this]
        {
            if (!syncing)
                main.remoteSelectPattern(static_cast<std::size_t>(
                    std::max(0, pattern.getSelectedItemIndex())));
        };
        addAndMakeVisible(pattern);
        timing.addItemList({"GRID", "FREE", "JITTER"}, 1);
        timing.onChange = [this]
        {
            if (!syncing)
                main.remoteSetTiming(static_cast<navalha::TimingMode>(
                    std::max(0, timing.getSelectedItemIndex())));
        };
        addAndMakeVisible(timing);
        pitch.setRange(-12.0, 11.0, 1.0);
        pitch.setTextValueSuffix(" st");
        pitch.onValueChange = [this]
        {
            if (!syncing)
                main.remoteSetPitch(
                    static_cast<int>(std::lround(pitch.getValue())));
        };
        addAndMakeVisible(pitch);
        master.setRange(0.0, 1.0, 0.01);
        master.onValueChange = [this]
        {
            if (!syncing)
                main.remoteSetMaster(master.getValue());
        };
        addAndMakeVisible(master);
        balance.setRange(-1.0, 1.0, 0.01);
        balance.setName("Source balance");
        balance.onValueChange = [this]
        {
            if (!syncing)
                main.remoteSetBalance(balance.getValue());
        };
        addAndMakeVisible(balance);
        const std::array<juce::String, 6> parameterNames {
            "BPM", "PATTERN", "TIMING", "PITCH", "MASTER", "A/B BALANCE"};
        for (std::size_t index = 0; index < parameterCaptions.size(); ++index)
            configureCaption(parameterCaptions[index], parameterNames[index]);

        assisted.setButtonText("AUTO");
        assisted.onClick = [this]
        {
            if (!syncing)
                main.remoteSetAssisted(assisted.getToggleState());
        };
        addAndMakeVisible(assisted);
        repeat.setButtonText("REPEAT");
        repeat.onClick = [this]
        {
            if (!syncing)
                main.remoteSetRepeat(repeat.getToggleState());
        };
        addAndMakeVisible(repeat);

        configureHeading(orderHeading, "PATTERN ORDER");
        const std::array<juce::String, 8> orderNames {
            "RANDOM A", "RANDOM B", "RANDOM A+B", "INTERLEAVE",
            juce::String::fromUTF8("0→7"), juce::String::fromUTF8("7→0"),
            "ZERO", "GAP"};
        const std::array<PatternMacro, 8> macros {
            PatternMacro::randomA, PatternMacro::randomB,
            PatternMacro::randomAB, PatternMacro::interleave,
            PatternMacro::forward, PatternMacro::reverse,
            PatternMacro::zero, PatternMacro::gap};
        for (std::size_t index = 0; index < orderButtons.size(); ++index)
            configure(
                orderButtons[index], orderNames[index],
                [this, macro = macros[index]]
                {
                    main.remoteApplyPatternMacro(macro);
                });

        configureHeading(liveHeading, "LIVE GESTURES");
        configure(stutter, "STUTTER x4", [this] { main.remoteStutter(); });
        configure(burst, "BURST x8", [this] { main.remoteBurst(); });
        configure(micro, "MICRO x8", [this] { main.remoteMicro(); });
        configure(reverse, "REVERSE", [this] { main.remoteReverse(); });
        configure(commit, "COMMIT", [this] { main.remoteCommitTransform(); });
        configure(restore, "RESTORE", [this] { main.remoteRestoreTransform(); });
        configure(formArm, "ARM FORM", [this] { main.remoteToggleForm(); });
        configure(formHold, "HOLD", [this] { main.remoteToggleFormHold(); });
        configure(formNext, "FORM NEXT", [this] { main.remoteNextForm(); });
        configure(formReset, "FORM RESET", [this] { main.remoteResetForm(); });

        xyPad.onMove = [this] (double bpm, int semitones)
        {
            main.remoteSetTempo(bpm);
            main.remoteSetPitch(semitones);
        };
        addAndMakeVisible(xyPad);

        status.setJustificationType(juce::Justification::centred);
        status.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        addAndMakeVisible(status);
        for (std::size_t step = 0; step < steps.size(); ++step)
        {
            steps[step].setText(
                "STEP " + juce::String(step + 1),
                juce::dontSendNotification);
            steps[step].setJustificationType(juce::Justification::centred);
            steps[step].setColour(
                juce::Label::backgroundColourId,
                juce::Colour(Arcade::surfaceHigh));
            steps[step].setColour(
                juce::Label::outlineColourId, juce::Colour(Arcade::line));
            addAndMakeVisible(steps[step]);
        }

        getProperties().set("learnKey", "dual");
        title.getProperties().set("learnKey", "dual");
        status.getProperties().set("learnKey", "dual");
        play.getProperties().set("learnKey", "play");
        stop.getProperties().set("learnKey", "stop");
        reset.getProperties().set("learnKey", "reset");
        next.getProperties().set("learnKey", "assisted");
        record.getProperties().set("learnKey", "rec");
        sourceA.getProperties().set("learnKey", "mixer");
        sourceB.getProperties().set("learnKey", "mixer");
        assisted.getProperties().set("learnKey", "assisted");
        repeat.getProperties().set("learnKey", "assisted");
        tempo.getProperties().set("learnKey", "bpm");
        pattern.getProperties().set("learnKey", "pattern");
        timing.getProperties().set("learnKey", "timing");
        pitch.getProperties().set("learnKey", "pitch");
        master.getProperties().set("learnKey", "output");
        balance.getProperties().set("learnKey", "mixerbalance");
        for (std::size_t index = 0; index < parameterCaptions.size(); ++index)
        {
            const std::array<const char*, 6> keys {
                "bpm", "pattern", "timing", "pitch", "output",
                "mixerbalance"};
            parameterCaptions[index].getProperties().set(
                "learnKey", keys[index]);
        }
        orderHeading.getProperties().set("learnKey", "order");
        for (auto& button : orderButtons)
            button.getProperties().set("learnKey", "order");
        liveHeading.getProperties().set("learnKey", "gesture");
        for (auto* button : {
                 &stutter, &burst, &micro, &reverse, &commit, &restore})
            button->getProperties().set("learnKey", "gesture");
        for (auto* button : {&formArm, &formHold, &formNext, &formReset})
            button->getProperties().set("learnKey", "form");
        xyPad.getProperties().set("learnKey", "trace");
    }

    ~PerformanceRemoteComponent() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    void visibilityChanged() override
    {
        if (isShowing())
        {
            timerCallback();
            startTimerHz(30);
        }
        else
        {
            stopTimer();
        }
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(Arcade::background));
        graphics.setColour(juce::Colour(Arcade::yellow));
        graphics.drawRect(getLocalBounds().toFloat().reduced(8.5F), 1.0F);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(18);
        title.setBounds(area.removeFromTop(54));
        auto transport = area.removeFromTop(58);
        stop.setBounds(transport.removeFromLeft(90).reduced(4));
        play.setBounds(transport.removeFromLeft(90).reduced(4));
        reset.setBounds(transport.removeFromLeft(90).reduced(4));
        record.setBounds(transport.removeFromLeft(90).reduced(4));
        sourceA.setBounds(transport.removeFromLeft(85).reduced(4));
        sourceB.setBounds(transport.removeFromLeft(85).reduced(4));
        assisted.setBounds(transport.removeFromLeft(90).reduced(4));
        repeat.setBounds(transport.removeFromLeft(100).reduced(4));
        next.setBounds(transport.removeFromLeft(90).reduced(4));
        status.setBounds(transport.reduced(4));

        auto parameters = area.removeFromTop(58);
        std::array<juce::Component*, 6> parameterControls {
            &tempo, &pattern, &timing, &pitch, &master, &balance};
        const std::array<int, 5> parameterWidths {220, 100, 120, 200, 200};
        for (std::size_t index = 0; index < parameterControls.size(); ++index)
        {
            auto parameterArea = index + 1 < parameterControls.size()
                ? parameters.removeFromLeft(parameterWidths[index])
                : parameters;
            parameterCaptions[index].setBounds(
                parameterArea.removeFromTop(16).reduced(4, 0));
            parameterControls[index]->setBounds(parameterArea.reduced(4, 1));
        }

        auto stepRow = area.removeFromTop(52);
        const auto stepWidth =
            stepRow.getWidth() / static_cast<int>(steps.size());
        for (auto& step : steps)
            step.setBounds(stepRow.removeFromLeft(stepWidth).reduced(3));

        auto orderRow = area.removeFromTop(48);
        orderHeading.setBounds(orderRow.removeFromLeft(105).reduced(3));
        const auto orderWidth =
            orderRow.getWidth() / static_cast<int>(orderButtons.size());
        for (auto& button : orderButtons)
            button.setBounds(orderRow.removeFromLeft(orderWidth).reduced(2, 3));

        auto liveRow = area.removeFromTop(48);
        liveHeading.setBounds(liveRow.removeFromLeft(105).reduced(3));
        std::array<juce::TextButton*, 10> liveButtons {
            &stutter, &burst, &micro, &reverse, &commit, &restore,
            &formArm, &formHold, &formNext, &formReset};
        const auto liveWidth =
            liveRow.getWidth() / static_cast<int>(liveButtons.size());
        for (auto* button : liveButtons)
            button->setBounds(liveRow.removeFromLeft(liveWidth).reduced(2, 3));
        xyPad.setBounds(area.reduced(4));
    }

private:
    void configure(juce::TextButton& button,
                   const juce::String& text,
                   std::function<void()> action)
    {
        button.setButtonText(text);
        button.onClick = std::move(action);
        addAndMakeVisible(button);
    }

    void configureHeading(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        label.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surfaceHigh));
        label.setColour(
            juce::Label::outlineColourId, juce::Colour(Arcade::line));
        addAndMakeVisible(label);
    }

    void configureCaption(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        addAndMakeVisible(label);
    }

    void timerCallback() override
    {
        const auto state = main.performanceSnapshot();
        syncing = true;
        tempo.setValue(state.bpm, juce::dontSendNotification);
        pattern.setSelectedItemIndex(
            static_cast<int>(state.pattern), juce::dontSendNotification);
        timing.setSelectedItemIndex(
            static_cast<int>(state.timing), juce::dontSendNotification);
        pitch.setValue(state.pitch, juce::dontSendNotification);
        master.setValue(state.master, juce::dontSendNotification);
        balance.setValue(state.balance, juce::dontSendNotification);
        assisted.setToggleState(
            state.assisted, juce::dontSendNotification);
        repeat.setToggleState(state.repeat, juce::dontSendNotification);
        sourceA.setToggleState(state.source == 0, juce::dontSendNotification);
        sourceB.setToggleState(state.source == 1, juce::dontSendNotification);
        record.setToggleState(state.recording, juce::dontSendNotification);
        record.setButtonText(state.recording ? "STOP REC" : "REC");
        formArm.setButtonText(state.formEnabled ? "FORM ON" : "ARM FORM");
        formHold.setButtonText(state.formHold ? "RELEASE" : "HOLD");
        play.setToggleState(state.running, juce::dontSendNotification);
        stop.setToggleState(!state.running, juce::dontSendNotification);
        xyPad.setValues(state.bpm, state.pitch);
        status.setText(
            juce::String(state.running ? "PLAY" : "STOP")
                + " | PATTERN " + juce::String(state.pattern + 1)
                + " | SOURCE " + (state.source == 0 ? "A" : "B")
                + " | FORM " + juce::String(state.formScene + 1)
                + " | " + juce::String(std::lround(state.bpm)) + " BPM",
            juce::dontSendNotification);
        for (std::size_t step = 0; step < steps.size(); ++step)
            steps[step].setColour(
                juce::Label::backgroundColourId,
                juce::Colour(
                    state.running && state.step == step
                        ? Arcade::yellow : Arcade::surfaceHigh));
        syncing = false;
    }

    MainComponent& main;
    ArcadeLookAndFeel lookAndFeel;
    juce::Label title;
    juce::Label status;
    juce::TextButton play;
    juce::TextButton stop;
    juce::TextButton reset;
    juce::TextButton next;
    juce::TextButton record;
    juce::TextButton sourceA;
    juce::TextButton sourceB;
    juce::ToggleButton assisted;
    juce::ToggleButton repeat;
    juce::Slider tempo {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::ComboBox pattern;
    juce::ComboBox timing;
    juce::Slider pitch {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider master {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Slider balance {
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    std::array<juce::Label, 6> parameterCaptions;
    std::array<juce::Label, navalha::stepsPerPattern> steps;
    juce::Label orderHeading;
    juce::Label liveHeading;
    std::array<juce::TextButton, 8> orderButtons;
    juce::TextButton stutter;
    juce::TextButton burst;
    juce::TextButton micro;
    juce::TextButton reverse;
    juce::TextButton commit;
    juce::TextButton restore;
    juce::TextButton formArm;
    juce::TextButton formHold;
    juce::TextButton formNext;
    juce::TextButton formReset;
    PerformanceXYPad xyPad;
    bool syncing = false;
};

class MasterMetricsList final : public juce::Component
{
public:
    MasterMetricsList()
    {
        const std::array<juce::String, 6> labels {
            "PEAK", "RMS", "ESTIMATED LUFS",
            "CREST", "CORRELATION", "HEADROOM"};
        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            rows[index].label = labels[index];
            rows[index].value = juce::String::charToString(0x2014);
        }
        rows[0].unit = "dBFS";
        rows[1].unit = "dBFS";
        rows[3].unit = "dB";
        rows[5].unit = "dB";
    }

    void setValues(const std::array<juce::String, 6>& values)
    {
        for (std::size_t index = 0; index < rows.size(); ++index)
            rows[index].value = values[index];
        repaint();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(Arcade::surface));
        auto area = getLocalBounds();
        graphics.setColour(juce::Colour(Arcade::line));
        graphics.drawRect(area, 1);

        auto header = area.reduced(1).removeFromTop(42);
        graphics.setColour(juce::Colour(Arcade::surfaceHigh));
        graphics.fillRect(header);
        graphics.setColour(juce::Colour(Arcade::yellow));
        graphics.fillRect(header.removeFromLeft(5));
        graphics.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 12.0F, juce::Font::bold)));
        graphics.drawFittedText(
            "TRACK ANALYSIS", header.reduced(10, 2),
            juce::Justification::centredLeft, 1);

        area = getLocalBounds().reduced(1);
        area.removeFromTop(42);
        const auto rowHeight = std::max(
            1, area.getHeight() / static_cast<int>(rows.size()));
        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            auto row = index + 1 < rows.size()
                ? area.removeFromTop(rowHeight) : area;
            if (index % 2 != 0)
            {
                graphics.setColour(
                    juce::Colour(Arcade::surfaceHigh).withAlpha(0.55F));
                graphics.fillRect(row);
            }
            graphics.setColour(juce::Colour(Arcade::line));
            graphics.drawHorizontalLine(
                row.getBottom() - 1,
                static_cast<float>(row.getX() + 8),
                static_cast<float>(row.getRight() - 8));

            auto content = row.reduced(12, 7);
            auto labelArea = content.removeFromTop(17);
            graphics.setColour(juce::Colour(Arcade::muted));
            graphics.setFont(juce::Font(juce::FontOptions(
                "DejaVu Sans Mono", 10.0F, juce::Font::bold)));
            graphics.drawFittedText(
                rows[index].label, labelArea,
                juce::Justification::centredLeft, 1);

            auto unitArea = content.removeFromRight(48);
            graphics.setColour(juce::Colour(Arcade::muted));
            graphics.setFont(juce::Font(juce::FontOptions(
                "DejaVu Sans Mono", 9.0F, juce::Font::plain)));
            graphics.drawFittedText(
                rows[index].unit, unitArea,
                juce::Justification::centredRight, 1);
            graphics.setColour(juce::Colour(Arcade::yellowHigh));
            graphics.setFont(juce::Font(juce::FontOptions(
                "DejaVu Sans Mono", 18.0F, juce::Font::bold)));
            graphics.drawFittedText(
                rows[index].value, content,
                juce::Justification::centredRight, 1);
        }
    }

private:
    struct Row
    {
        juce::String label;
        juce::String value;
        juce::String unit;
    };

    std::array<Row, 6> rows;
};

class MasteringComponent final : public juce::Component
{
public:
    MasteringComponent()
    {
        setLookAndFeel(&lookAndFeel);
        title.setText(
            "MASTER | INTERNAL ESTIMATES",
            juce::dontSendNotification);
        title.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 18.0F, juce::Font::bold)));
        title.getProperties().set("arcadeFontSize", 18.0);
        title.getProperties().set("arcadeFontBold", true);
        title.setJustificationType(juce::Justification::centred);
        title.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        title.getProperties().set("arcadeTitle", true);
        addAndMakeVisible(title);

        mode.addItemList({"TRACK MASTER", "ALBUM MASTER"}, 1);
        mode.setSelectedItemIndex(0, juce::dontSendNotification);
        mode.onChange = [this] { updateMode(); };
        addAndMakeVisible(mode);

        configure(loadTrack, "LOAD TRACK", [this] { chooseTrack(); });
        configure(analyzeTrack, "ANALYZE", [this] { analyzeLoadedTrack(); });
        configure(renderTrack, "RENDER PCM24", [this] { chooseTrackOutput(); });
        configure(loadRecipe, "LOAD RECIPE", [this] { chooseRecipe(); });
        configure(saveRecipe, "SAVE RECIPE", [this] { chooseRecipeOutput(); });
        configure(loadAlbum, "LOAD MANIFEST", [this] { chooseAlbum(); });
        configure(renderAlbum, "RENDER ALBUM", [this] { chooseAlbumOutput(); });

        sourceInfo.setText(
            "No TRACK MASTER source loaded", juce::dontSendNotification);
        sourceInfo.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        sourceInfo.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(sourceInfo);
        addAndMakeVisible(metrics);

        configureParameter(trim, -12.0, 12.0, 0.1, "TRIM dB");
        configureParameter(highPass, 15.0, 120.0, 1.0, "HPF Hz");
        configureParameter(lowShelf, -6.0, 6.0, 0.1, "LOW dB");
        configureParameter(presence, -6.0, 6.0, 0.1, "PRESENCE dB");
        configureParameter(highShelf, -6.0, 6.0, 0.1, "HIGH dB");
        configureParameter(threshold, -30.0, -2.0, 0.1, "THRESHOLD dB");
        configureParameter(ratio, 1.0, 8.0, 0.1, "RATIO");
        configureParameter(width, 0.0, 2.0, 0.01, "WIDTH");
        configureParameter(saturation, 0.0, 1.0, 0.01, "SATURATION");
        configureParameter(ceiling, -6.0, 0.0, 0.1, "CEILING dB");
        setParameters({});

        albumInfo.setMultiLine(true);
        albumInfo.setReadOnly(true);
        albumInfo.setScrollbarsShown(true);
        albumInfo.setColour(
            juce::TextEditor::backgroundColourId, juce::Colour(Arcade::surface));
        albumInfo.setColour(
            juce::TextEditor::textColourId, juce::Colour(Arcade::ink));
        albumInfo.setText("No ALBUM MASTER manifest loaded", false);
        addAndMakeVisible(albumInfo);

        status.setText(
            "MASTER is supplementary and never owns the realtime engine.",
            juce::dontSendNotification);
        status.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        status.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(status);
        getProperties().set("learnKey", "masterwindow");
        updateMode();
    }

    ~MasteringComponent() override
    {
        setLookAndFeel(nullptr);
    }

    bool loadTrackFile(const juce::File& file)
    {
        if (busy.load() || !file.existsAsFile())
        {
            setStatus("TRACK LOAD FAILED | FILE NOT AVAILABLE");
            return false;
        }
        try
        {
            auto decoded = decodeSourceFile(file);
            source = std::move(decoded.audio);
            sourceFile = file;
            mode.setSelectedItemIndex(0, juce::dontSendNotification);
            updateMode();
            sourceInfo.setText(
                file.getFileName() + " | "
                    + juce::String(source->sampleRate(), 0) + " Hz | "
                    + juce::String(source->size()) + " frames",
                juce::dontSendNotification);
            analyzeLoadedTrack();
            return true;
        }
        catch (const std::exception& exception)
        {
            setStatus("TRACK LOAD FAILED | "
                      + juce::String(exception.what()));
            return false;
        }
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(tintedPanelSurface(
            juce::Colour(Arcade::yellow), 0.14F));
        graphics.setColour(juce::Colour(Arcade::yellow));
        graphics.drawRect(getLocalBounds().toFloat().reduced(8.5F), 1.0F);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(18);
        title.setBounds(area.removeFromTop(52));
        mode.setBounds(area.removeFromTop(42).removeFromLeft(230).reduced(3));

        auto actions = area.removeFromTop(50);
        loadTrack.setBounds(actions.removeFromLeft(130).reduced(3));
        analyzeTrack.setBounds(actions.removeFromLeft(120).reduced(3));
        renderTrack.setBounds(actions.removeFromLeft(150).reduced(3));
        loadRecipe.setBounds(actions.removeFromLeft(135).reduced(3));
        saveRecipe.setBounds(actions.removeFromLeft(135).reduced(3));
        loadAlbum.setBounds(actions.removeFromLeft(155).reduced(3));
        renderAlbum.setBounds(actions.removeFromLeft(155).reduced(3));
        sourceInfo.setBounds(area.removeFromTop(44).reduced(3));

        auto trackArea = area.removeFromTop(430);
        metrics.setBounds(trackArea.removeFromLeft(310).reduced(3));
        auto parameters = trackArea.reduced(6);
        const auto leftWidth = parameters.getWidth() / 2;
        auto left = parameters.removeFromLeft(leftWidth);
        auto right = parameters;
        const auto place = [] (juce::Slider& slider,
                               juce::Rectangle<int>& column)
        {
            slider.setBounds(column.removeFromTop(70).reduced(5));
        };
        place(trim, left);
        place(highPass, left);
        place(lowShelf, left);
        place(presence, left);
        place(highShelf, left);
        place(threshold, right);
        place(ratio, right);
        place(width, right);
        place(saturation, right);
        place(ceiling, right);

        albumInfo.setBounds(area.removeFromTop(
            std::max(120, area.getHeight() - 42)).reduced(3));
        status.setBounds(area.reduced(3));
    }

private:
    void configure(juce::TextButton& button,
                   const juce::String& text,
                   std::function<void()> action)
    {
        button.setButtonText(text);
        button.onClick = std::move(action);
        addAndMakeVisible(button);
    }

    void configureParameter(juce::Slider& slider,
                            double minimum,
                            double maximum,
                            double interval,
                            const juce::String& name)
    {
        slider.setName(name);
        slider.setRange(minimum, maximum, interval);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(
            juce::Slider::TextBoxRight, false, 70, 24);
        addAndMakeVisible(slider);
    }

    [[nodiscard]] navalha::MasteringParameters parameters() const
    {
        return {
            trim.getValue(),
            highPass.getValue(),
            lowShelf.getValue(),
            presence.getValue(),
            highShelf.getValue(),
            threshold.getValue(),
            ratio.getValue(),
            width.getValue(),
            saturation.getValue(),
            ceiling.getValue()};
    }

    void setParameters(const navalha::MasteringParameters& value)
    {
        trim.setValue(value.trimDb, juce::dontSendNotification);
        highPass.setValue(value.highPassHz, juce::dontSendNotification);
        lowShelf.setValue(value.lowShelfDb, juce::dontSendNotification);
        presence.setValue(value.presenceDb, juce::dontSendNotification);
        highShelf.setValue(value.highShelfDb, juce::dontSendNotification);
        threshold.setValue(
            value.compressorThresholdDb, juce::dontSendNotification);
        ratio.setValue(value.compressorRatio, juce::dontSendNotification);
        width.setValue(value.width, juce::dontSendNotification);
        saturation.setValue(value.saturation, juce::dontSendNotification);
        ceiling.setValue(value.ceilingDb, juce::dontSendNotification);
    }

    void updateMode()
    {
        const auto trackMode = mode.getSelectedItemIndex() == 0;
        for (auto* component : {
                 static_cast<juce::Component*>(&loadTrack),
                 static_cast<juce::Component*>(&analyzeTrack),
                 static_cast<juce::Component*>(&renderTrack),
                 static_cast<juce::Component*>(&loadRecipe),
                 static_cast<juce::Component*>(&saveRecipe),
                 static_cast<juce::Component*>(&sourceInfo),
                 static_cast<juce::Component*>(&metrics),
                 static_cast<juce::Component*>(&trim),
                 static_cast<juce::Component*>(&highPass),
                 static_cast<juce::Component*>(&lowShelf),
                 static_cast<juce::Component*>(&presence),
                 static_cast<juce::Component*>(&highShelf),
                 static_cast<juce::Component*>(&threshold),
                 static_cast<juce::Component*>(&ratio),
                 static_cast<juce::Component*>(&width),
                 static_cast<juce::Component*>(&saturation),
                 static_cast<juce::Component*>(&ceiling)})
            component->setVisible(trackMode);
        loadAlbum.setVisible(!trackMode);
        renderAlbum.setVisible(!trackMode);
        albumInfo.setVisible(!trackMode);
    }

    void chooseTrack()
    {
        if (busy.load())
            return;
        chooser = std::make_unique<juce::FileChooser>(
            "Load TRACK MASTER source", juce::File {},
            "*.wav;*.wave;*.aif;*.aiff");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& selected)
            {
                const auto file = selected.getResult();
                if (file.existsAsFile())
                    static_cast<void>(loadTrackFile(file));
                chooser.reset();
            });
    }

    void analyzeLoadedTrack()
    {
        if (source == nullptr)
        {
            setStatus("LOAD A TRACK BEFORE ANALYSIS");
            return;
        }
        try
        {
            const auto value = navalha::analyzeForMastering(*source);
            metrics.setValues({
                juce::String(value.peakDb, 3),
                juce::String(value.rmsDb, 3),
                juce::String(value.estimatedLufs, 3),
                juce::String(value.crestDb, 3),
                juce::String(value.correlation, 4),
                juce::String(value.headroomDb, 3)});
            setStatus("TRACK ANALYZED | INTERNAL ESTIMATE, NOT EBU CERTIFIED");
        }
        catch (const std::exception& exception)
        {
            setStatus("ANALYSIS FAILED | " + juce::String(exception.what()));
        }
    }

    void chooseTrackOutput()
    {
        if (busy.load() || source == nullptr)
        {
            if (source == nullptr)
                setStatus("LOAD A TRACK BEFORE RENDER");
            return;
        }
        chooser = std::make_unique<juce::FileChooser>(
            "Render TRACK MASTER",
            sourceFile.getSiblingFile(
                sourceFile.getFileNameWithoutExtension() + "_MASTER.wav"),
            "*.wav");
        chooser->launchAsync(
            juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& selected)
            {
                auto output = selected.getResult();
                if (output != juce::File {})
                {
                    if (!output.hasFileExtension("wav"))
                        output = output.withFileExtension("wav");
                    startTrackRender(output);
                }
                chooser.reset();
            });
    }

    void startTrackRender(const juce::File& output)
    {
        auto audio = *source;
        const auto settings = parameters();
        const auto inputName = sourceFile.getFileName().toStdString();
        busy.store(true);
        setStatus("TRACK MASTER RENDERING...");
        juce::Component::SafePointer<MasteringComponent> safe(this);
        if (!juce::Thread::launch(
                [safe, audio = std::move(audio), settings, output, inputName] () mutable
                {
                    juce::String result;
                    try
                    {
                        const auto rendered =
                            navalha::renderMastering(audio, settings);
                        publishMasterWav(
                            output, audio.sampleRate(), rendered,
                            {"Navalha 2 TRACK MASTER", "", inputName, "",
                             "C++ mastering chain; internal-estimate-not-EBU-certified"});
                        result = "TRACK MASTER READY | " + output.getFileName();
                    }
                    catch (const std::exception& exception)
                    {
                        result = "TRACK MASTER FAILED | "
                            + juce::String(exception.what());
                    }
                    juce::MessageManager::callAsync([safe, result]
                    {
                        if (safe != nullptr)
                        {
                            safe->busy.store(false);
                            safe->setStatus(result);
                        }
                    });
                }))
        {
            busy.store(false);
            setStatus("UNABLE TO START MASTER WORKER");
        }
    }

    void chooseRecipe()
    {
        chooser = std::make_unique<juce::FileChooser>(
            "Load MASTER recipe", juce::File {}, "*.json");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& selected)
            {
                const auto file = selected.getResult();
                if (file.existsAsFile() && file.getSize() <= 1024 * 1024)
                {
                    try
                    {
                        const auto recipe = navalha::decodeMasteringRecipe(
                            file.loadFileAsString().toStdString());
                        setParameters(recipe.parameters);
                        setStatus("MASTER RECIPE LOADED | " + file.getFileName());
                    }
                    catch (const std::exception& exception)
                    {
                        setStatus("RECIPE LOAD FAILED | "
                                  + juce::String(exception.what()));
                    }
                }
                chooser.reset();
            });
    }

    void chooseRecipeOutput()
    {
        chooser = std::make_unique<juce::FileChooser>(
            "Save MASTER recipe", juce::File {}, "*.json");
        chooser->launchAsync(
            juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& selected)
            {
                auto file = selected.getResult();
                if (file != juce::File {})
                {
                    if (!file.hasFileExtension("json"))
                        file = file.withFileExtension("json");
                    const navalha::MasteringRecipe recipe {
                        sourceFile.getFileName().toStdString(),
                        juce::Time::getCurrentTime().toISO8601(true).toStdString(),
                        parameters()};
                    const auto ok = file.replaceWithText(
                        navalha::encodeMasteringRecipe(recipe));
                    setStatus(ok ? "MASTER RECIPE SAVED | " + file.getFileName()
                                 : "MASTER RECIPE SAVE FAILED");
                }
                chooser.reset();
            });
    }

    void chooseAlbum()
    {
        chooser = std::make_unique<juce::FileChooser>(
            "Load ALBUM MASTER manifest", juce::File {}, "*.json");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& selected)
            {
                const auto file = selected.getResult();
                if (file.existsAsFile())
                {
                    try
                    {
                        if (file.getSize() > 4 * 1024 * 1024)
                            throw std::length_error(
                                "Manifest exceeds the 4 MiB safety limit");
                        album = navalha::decodeAlbumMasterManifest(
                            file.loadFileAsString().toStdString());
                        albumFile = file;
                        juce::String description;
                        description << album.title << "\n"
                                    << album.artist << "\n"
                                    << album.tracks.size() << " tracks\n\n";
                        for (std::size_t index = 0;
                             index < album.tracks.size(); ++index)
                            description << juce::String(index + 1).paddedLeft('0', 2)
                                        << " | "
                                        << juce::String(album.tracks[index].title)
                                        << " | "
                                        << juce::String(album.tracks[index].filename)
                                        << "\n";
                        albumInfo.setText(description, false);
                        setStatus("ALBUM MANIFEST LOADED | "
                                  + juce::String(album.tracks.size()) + " TRACKS");
                    }
                    catch (const std::exception& exception)
                    {
                        setStatus("ALBUM LOAD FAILED | "
                                  + juce::String(exception.what()));
                    }
                }
                chooser.reset();
            });
    }

    void chooseAlbumOutput()
    {
        if (busy.load() || album.tracks.empty())
        {
            if (album.tracks.empty())
                setStatus("LOAD AN ALBUM MANIFEST BEFORE RENDER");
            return;
        }
        chooser = std::make_unique<juce::FileChooser>(
            "Choose ALBUM MASTER output directory",
            albumFile.getParentDirectory(), "*");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& selected)
            {
                const auto directory = selected.getResult();
                if (directory.isDirectory())
                    startAlbumRender(directory);
                chooser.reset();
            });
    }

    void startAlbumRender(const juce::File& outputDirectory)
    {
        const auto manifest = album;
        const auto manifestDirectory = albumFile.getParentDirectory();
        busy.store(true);
        setStatus("ALBUM MASTER RENDERING...");
        juce::Component::SafePointer<MasteringComponent> safe(this);
        if (!juce::Thread::launch(
                [safe, manifest, manifestDirectory, outputDirectory]
                {
                    juce::String result;
                    try
                    {
                        std::vector<std::pair<juce::File, juce::File>> paths;
                        paths.reserve(manifest.tracks.size());
                        for (std::size_t index = 0;
                             index < manifest.tracks.size(); ++index)
                        {
                            const auto& track = manifest.tracks[index];
                            const auto sourcePath = manifestDirectory.getChildFile(
                                juce::String(track.filename));
                            if (!sourcePath.existsAsFile())
                                throw std::runtime_error(
                                    "Missing album track: " + track.filename);
                            const auto prefix =
                                juce::String(index + 1).paddedLeft('0', 2) + "_";
                            const auto outputPath = outputDirectory.getChildFile(
                                prefix + safeMasterStem(
                                    juce::String(track.title))
                                    + "_MASTER.wav");
                            if (outputPath.existsAsFile()
                                || juce::File(
                                    outputPath.getFullPathName() + ".partial")
                                    .exists())
                                throw std::runtime_error(
                                    "Album output already exists");
                            paths.emplace_back(sourcePath, outputPath);
                        }
                        for (std::size_t index = 0;
                             index < manifest.tracks.size(); ++index)
                        {
                            auto decoded = decodeSourceFile(paths[index].first);
                            auto settings = manifest.chain;
                            settings.trimDb +=
                                manifest.tracks[index].settings.trimDb;
                            const auto rendered = navalha::renderMastering(
                                *decoded.audio, settings);
                            publishMasterWav(
                                paths[index].second,
                                decoded.audio->sampleRate(),
                                rendered,
                                {manifest.tracks[index].title,
                                 manifest.artist,
                                 manifest.title,
                                 "",
                                 "Navalha 2 C++ ALBUM MASTER batch render"});
                        }
                        result = "ALBUM MASTER READY | "
                            + juce::String(manifest.tracks.size()) + " TRACKS";
                    }
                    catch (const std::exception& exception)
                    {
                        result = "ALBUM MASTER FAILED | "
                            + juce::String(exception.what());
                    }
                    juce::MessageManager::callAsync([safe, result]
                    {
                        if (safe != nullptr)
                        {
                            safe->busy.store(false);
                            safe->setStatus(result);
                        }
                    });
                }))
        {
            busy.store(false);
            setStatus("UNABLE TO START ALBUM WORKER");
        }
    }

    void setStatus(const juce::String& message)
    {
        status.setText(message, juce::dontSendNotification);
    }

    ArcadeLookAndFeel lookAndFeel;
    juce::Label title;
    juce::Label sourceInfo;
    juce::Label status;
    juce::ComboBox mode;
    juce::TextButton loadTrack;
    juce::TextButton analyzeTrack;
    juce::TextButton renderTrack;
    juce::TextButton loadRecipe;
    juce::TextButton saveRecipe;
    juce::TextButton loadAlbum;
    juce::TextButton renderAlbum;
    MasterMetricsList metrics;
    juce::TextEditor albumInfo;
    juce::Slider trim;
    juce::Slider highPass;
    juce::Slider lowShelf;
    juce::Slider presence;
    juce::Slider highShelf;
    juce::Slider threshold;
    juce::Slider ratio;
    juce::Slider width;
    juce::Slider saturation;
    juce::Slider ceiling;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<navalha::StereoAudioBuffer> source;
    juce::File sourceFile;
    navalha::AlbumMasterManifest album;
    juce::File albumFile;
    std::atomic<bool> busy {false};
};

class ProductionWorkspaceComponent final : public juce::Component
{
public:
    explicit ProductionWorkspaceComponent(MainComponent& main)
        : takes(main, [this] (const juce::File& file)
          {
              if (mastering.loadTrackFile(file))
              {
                  selectedPage = 1;
                  updatePageVisibility();
              }
          })
    {
        setLookAndFeel(&lookAndFeel);
        heading.setText(
            "TAKES / MASTER",
            juce::dontSendNotification);
        heading.setFont(juce::Font(
            juce::FontOptions("DejaVu Sans Mono", 18.0F, juce::Font::bold)));
        heading.getProperties().set("arcadeFontSize", 18.0);
        heading.getProperties().set("arcadeFontBold", true);
        heading.setJustificationType(juce::Justification::centredLeft);
        heading.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        heading.getProperties().set("arcadeTitle", true);
        addAndMakeVisible(heading);

        configurePageButton(takesPage, "TAKE TIMELINE", 0, "takes");
        configurePageButton(masterPage, "MASTERING", 1, "masterwindow");
        addAndMakeVisible(takes);
        addAndMakeVisible(mastering);
        getProperties().set("learnKey", "production");
        setSize(1800, 920);
        updatePageVisibility();
    }

    ~ProductionWorkspaceComponent() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(Arcade::background));
        graphics.setColour(juce::Colour(Arcade::yellow));
        graphics.fillRect(0, 0, getWidth(), 4);
        graphics.setColour(juce::Colour(Arcade::line));
        graphics.drawRect(getLocalBounds().toFloat().reduced(0.5F));
        if (wideLayout)
        {
            const auto dividerX = takes.getRight() + 4;
            graphics.setColour(juce::Colour(Arcade::steel).withAlpha(0.82F));
            graphics.fillRect(dividerX, 50, 2, getHeight() - 58);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        auto header = area.removeFromTop(42);
        area.removeFromTop(6);
        wideLayout = getWidth() >= 1680;

        heading.setBounds(header.removeFromLeft(
            wideLayout ? header.getWidth() : juce::jmin(330, header.getWidth() / 2)));
        takesPage.setVisible(!wideLayout);
        masterPage.setVisible(!wideLayout);
        if (!wideLayout)
        {
            const auto pageWidth = header.getWidth() / 2;
            takesPage.setBounds(
                header.removeFromLeft(pageWidth).reduced(2));
            masterPage.setBounds(header.reduced(2));
            takes.setBounds(area);
            mastering.setBounds(area);
        }
        else
        {
            const auto takeWidth = area.getWidth() / 2;
            takes.setBounds(area.removeFromLeft(takeWidth));
            area.removeFromLeft(10);
            mastering.setBounds(area);
        }
        updatePageVisibility();
        repaint();
    }

private:
    void configurePageButton(juce::TextButton& button,
                             const juce::String& text,
                             int page,
                             const char* learnKey)
    {
        button.setButtonText(text);
        button.getProperties().set("learnKey", learnKey);
        button.onClick = [this, page]
        {
            selectedPage = page;
            updatePageVisibility();
        };
        addAndMakeVisible(button);
    }

    void updatePageVisibility()
    {
        takes.setVisible(wideLayout || selectedPage == 0);
        mastering.setVisible(wideLayout || selectedPage == 1);
        takesPage.setToggleState(
            selectedPage == 0, juce::dontSendNotification);
        masterPage.setToggleState(
            selectedPage == 1, juce::dontSendNotification);
    }

    ArcadeLookAndFeel lookAndFeel;
    juce::Label heading;
    juce::TextButton takesPage;
    juce::TextButton masterPage;
    MasteringComponent mastering;
    TakeTimelineComponent takes;
    int selectedPage = 0;
    bool wideLayout = true;
};

class TutorialComponent final : public juce::Component
{
public:
    explicit TutorialComponent(navalha::ui::Language initialLanguage)
        : language(initialLanguage)
    {
        heading.setJustificationType(juce::Justification::centredLeft);
        heading.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 20.0F, juce::Font::bold)));
        heading.getProperties().set("arcadeFontSize", 20.0);
        heading.getProperties().set("arcadeFontBold", true);
        heading.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        addAndMakeVisible(heading);

        contentsHeading.setJustificationType(juce::Justification::centredLeft);
        contentsHeading.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 12.0F, juce::Font::bold)));
        contentsHeading.getProperties().set("arcadeFontSize", 12.0);
        contentsHeading.getProperties().set("arcadeFontBold", true);
        contentsHeading.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        addAndMakeVisible(contentsHeading);
        for (std::size_t index = 0; index < contentsButtons.size(); ++index)
        {
            auto& button = contentsButtons[index];
            button.setClickingTogglesState(false);
            button.onClick = [this, index]
            {
                selectChapter(static_cast<int>(index));
            };
            addAndMakeVisible(button);
        }

        body.setMultiLine(true);
        body.setReadOnly(true);
        body.setCaretVisible(false);
        body.setScrollbarsShown(true);
        body.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 14.0F, juce::Font::plain)));
        body.setColour(
            juce::TextEditor::backgroundColourId, juce::Colour(Arcade::surface));
        body.setColour(
            juce::TextEditor::outlineColourId, juce::Colour(Arcade::line));
        body.setColour(
            juce::TextEditor::textColourId, juce::Colour(Arcade::ink));
        addAndMakeVisible(body);

        previous.setButtonText(juce::String::fromUTF8("← PREVIOUS"));
        previous.onClick = [this] { selectChapter(currentChapter - 1); };
        addAndMakeVisible(previous);
        next.setButtonText(juce::String::fromUTF8("NEXT →"));
        next.onClick = [this] { selectChapter(currentChapter + 1); };
        addAndMakeVisible(next);
        getProperties().set("learnKey", "tutorial");
        refresh();
        setSize(960, 600);
    }

    void setLanguage(navalha::ui::Language newLanguage)
    {
        language = newLanguage;
        refresh();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(Arcade::background));
        graphics.setColour(juce::Colour(Arcade::yellow));
        graphics.fillRect(0, 0, 6, getHeight());
        graphics.setColour(juce::Colour(Arcade::line));
        graphics.drawRect(getLocalBounds().reduced(10), 1);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(24);
        auto navigation = area.removeFromBottom(46);
        auto contents = area.removeFromLeft(246);
        area.removeFromLeft(12);
        contentsHeading.setBounds(contents.removeFromTop(34).reduced(4, 0));
        const auto contentsButtonHeight = contents.getHeight()
            / static_cast<int>(contentsButtons.size());
        for (auto& button : contentsButtons)
            button.setBounds(
                contents.removeFromTop(contentsButtonHeight).reduced(2));

        heading.setBounds(area.removeFromTop(48));
        area.removeFromTop(10);
        navigation.removeFromLeft(258);
        previous.setBounds(navigation.removeFromLeft(160).reduced(2));
        next.setBounds(navigation.removeFromRight(160).reduced(2));
        body.setBounds(area.reduced(2));
    }

private:
    void selectChapter(int index)
    {
        currentChapter = juce::jlimit(
            0, static_cast<int>(navalha::ui::tutorialChapters.size()) - 1,
            index);
        refresh();
    }

    void refresh()
    {
        const auto& value = navalha::ui::tutorialChapters[
            static_cast<std::size_t>(currentChapter)];
        heading.setText(
            navalha::ui::text(value.title, language),
            juce::dontSendNotification);
        const navalha::ui::LocalizedText contentsText {
            "CONTENTS", "SUMÁRIO", "SOMMAIRE", "SUMARIO"};
        contentsHeading.setText(
            navalha::ui::text(contentsText, language),
            juce::dontSendNotification);
        for (std::size_t index = 0; index < contentsButtons.size(); ++index)
        {
            contentsButtons[index].setButtonText(navalha::ui::text(
                navalha::ui::tutorialChapters[index].title, language));
            contentsButtons[index].setToggleState(
                static_cast<int>(index) == currentChapter,
                juce::dontSendNotification);
        }
        body.setText(navalha::ui::text(value.body, language), false);
        previous.setEnabled(currentChapter > 0);
        next.setEnabled(
            currentChapter + 1
            < static_cast<int>(navalha::ui::tutorialChapters.size()));
        const std::array<navalha::ui::LocalizedText, 2> buttonText {{
            {"← PREVIOUS", "← ANTERIOR", "← PRÉCÉDENT", "← ANTERIOR"},
            {"NEXT →", "PRÓXIMO →", "SUIVANT →", "SIGUIENTE →"}}};
        previous.setButtonText(navalha::ui::text(buttonText[0], language));
        next.setButtonText(navalha::ui::text(buttonText[1], language));
    }

    navalha::ui::Language language;
    int currentChapter = 0;
    juce::Label heading;
    juce::Label contentsHeading;
    std::array<juce::TextButton,
               navalha::ui::tutorialChapters.size()> contentsButtons;
    juce::TextEditor body;
    juce::TextButton previous;
    juce::TextButton next;
};

class TutorialWindow final : public juce::DocumentWindow
{
public:
    explicit TutorialWindow(navalha::ui::Language language)
        : DocumentWindow("Navalha 2 - TUTORIAL",
                         juce::Colours::black,
                         DocumentWindow::allButtons)
    {
        tutorial = new TutorialComponent(language);
        tutorial->setLookAndFeel(&lookAndFeel);
        setUsingNativeTitleBar(true);
        setIcon(navalhaAppIcon());
        setContentOwned(tutorial, true);
        setResizable(true, true);
        setResizeLimits(680, 440, 1300, 900);
        centreWithSize(960, 600);
    }

    ~TutorialWindow() override
    {
        if (tutorial != nullptr)
            tutorial->setLookAndFeel(nullptr);
    }

    void setLanguage(navalha::ui::Language language)
    {
        if (tutorial != nullptr)
            tutorial->setLanguage(language);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }

private:
    ArcadeLookAndFeel lookAndFeel;
    TutorialComponent* tutorial = nullptr;
};

class AppInfoComponent final : public juce::Component
{
public:
    explicit AppInfoComponent(navalha::ui::Language initialLanguage)
        : language(initialLanguage)
    {
        title.setText("NAVALHA 2", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centredLeft);
        title.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 26.0F, juce::Font::bold)));
        title.getProperties().set("arcadeFontSize", 26.0);
        title.getProperties().set("arcadeFontBold", true);
        title.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellowHigh));
        addAndMakeVisible(title);

        subtitle.setJustificationType(juce::Justification::centredLeft);
        subtitle.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 11.0F, juce::Font::bold)));
        subtitle.getProperties().set("arcadeFontSize", 11.0);
        subtitle.getProperties().set("arcadeFontBold", true);
        subtitle.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        addAndMakeVisible(subtitle);

        information.setMultiLine(true);
        information.setReadOnly(true);
        information.setCaretVisible(false);
        information.setScrollbarsShown(false);
        information.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 13.0F, juce::Font::plain)));
        information.setColour(
            juce::TextEditor::backgroundColourId, juce::Colour(Arcade::surface));
        information.setColour(
            juce::TextEditor::outlineColourId, juce::Colour(Arcade::line));
        information.setColour(
            juce::TextEditor::textColourId, juce::Colour(Arcade::ink));
        addAndMakeVisible(information);

        status.setJustificationType(juce::Justification::centredLeft);
        status.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 10.5F, juce::Font::bold)));
        status.getProperties().set("arcadeFontSize", 10.5);
        status.getProperties().set("arcadeFontBold", true);
        status.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::yellow));
        status.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::surfaceHigh));
        status.setColour(
            juce::Label::outlineColourId, juce::Colour(Arcade::line));
        addAndMakeVisible(status);
        getProperties().set("learnKey", "app");
        refresh();
        setSize(720, 480);
    }

    void setLanguage(navalha::ui::Language newLanguage)
    {
        language = newLanguage;
        refresh();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(Arcade::background));
        graphics.setColour(juce::Colour(Arcade::yellow));
        graphics.fillRect(0, 0, 7, getHeight());
        graphics.setColour(juce::Colour(Arcade::line));
        graphics.drawRect(getLocalBounds().reduced(12), 1);
        graphics.setColour(juce::Colour(Arcade::yellow).withAlpha(0.08F));
        graphics.fillRect(13, 13, getWidth() - 26, 84);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(30);
        title.setBounds(area.removeFromTop(42));
        subtitle.setBounds(area.removeFromTop(28));
        area.removeFromTop(16);
        status.setBounds(area.removeFromBottom(38));
        area.removeFromBottom(12);
        information.setBounds(area);
    }

private:
    void refresh()
    {
        subtitle.setText(
            navalha::ui::text(
                {"ABOUT / APP INFORMATION", "SOBRE / INFORMAÇÕES DO APP",
                 "À PROPOS / INFORMATIONS APP", "ACERCA DE / INFORMACIÓN APP"},
                language),
            juce::dontSendNotification);
        const std::array<navalha::ui::LocalizedText, 4> labels {{
            {"VERSION", "VERSÃO", "VERSION", "VERSIÓN"},
            {"REFERENCE", "REFERÊNCIA", "RÉFÉRENCE", "REFERENCIA"},
            {"AUTHORSHIP", "AUTORIA", "AUTEURS", "AUTORÍA"},
            {"LICENSES", "LICENÇAS", "LICENCES", "LICENCIAS"}}};
        juce::String value;
        value << navalha::ui::text(labels[0], language)
              << "\nJUCE v" JUCE_APPLICATION_VERSION_STRING "\n\n"
              << navalha::ui::text(labels[1], language)
              << "\nPure Data v0.28.1\n\n"
              << navalha::ui::text(labels[2], language)
              << juce::String::fromUTF8(
                     "\nNAVALHA — Glerm Soares, 2009\n"
                     "NAVALHA 2 — upgrade: Lúcio Araújo, 2026\n\n")
              << navalha::ui::text(labels[3], language)
              << "\nNavalha 2 source: GPL-3.0-or-later\n"
                 "JUCE framework: AGPL-3.0-only\n"
                 "Details: docs/LICENSE_STATUS.md";
        information.setText(value, false);
        status.setText(
            navalha::ui::text(
                {"DEVELOPMENT BUILD · parity with PD v0.28.1 is still partial",
                 "BUILD EM DESENVOLVIMENTO · a paridade com PD v0.28.1 ainda é parcial",
                 "BUILD EN DÉVELOPPEMENT · la parité avec PD v0.28.1 reste partielle",
                 "BUILD EN DESARROLLO · la paridad con PD v0.28.1 aún es parcial"},
                language),
            juce::dontSendNotification);
    }

    navalha::ui::Language language;
    juce::Label title;
    juce::Label subtitle;
    juce::TextEditor information;
    juce::Label status;
};

class AppInfoWindow final : public juce::DocumentWindow
{
public:
    explicit AppInfoWindow(navalha::ui::Language language)
        : DocumentWindow("Navalha 2 - ABOUT",
                         juce::Colours::black,
                         DocumentWindow::allButtons)
    {
        content = new AppInfoComponent(language);
        content->setLookAndFeel(&lookAndFeel);
        setUsingNativeTitleBar(true);
        setIcon(navalhaAppIcon());
        setContentOwned(content, true);
        setResizable(true, true);
        setResizeLimits(620, 410, 1100, 780);
        centreWithSize(720, 480);
    }

    ~AppInfoWindow() override
    {
        if (content != nullptr)
            content->setLookAndFeel(nullptr);
    }

    void setLanguage(navalha::ui::Language language)
    {
        if (content != nullptr)
            content->setLanguage(language);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }

private:
    ArcadeLookAndFeel lookAndFeel;
    AppInfoComponent* content = nullptr;
};

class NavigationContainer final : public juce::Component,
                                  public juce::DragAndDropContainer
{
public:
    NavigationContainer(std::function<void(MainComponent&, bool)> showPerform,
                        std::function<void(MainComponent&)> showProduction)
        : openPerform(std::move(showPerform)),
          openProduction(std::move(showProduction))
    {
        setLookAndFeel(&arcadeLookAndFeel);
        viewport.setScrollBarsShown(true, false);
        viewport.setScrollBarThickness(10);
        viewport.getVerticalScrollBar().setColour(
            juce::ScrollBar::backgroundColourId,
            juce::Colour(Arcade::background));
        viewport.getVerticalScrollBar().setColour(
            juce::ScrollBar::thumbColourId,
            juce::Colour(Arcade::steel).withAlpha(0.72F));
        mainContent = new MainComponent();
        dualMonitorAvailable = hasDualMonitorSpace();
        dualMonitorRequested = dualMonitorAvailable
            && mainContent->prefersDualMonitor();
        viewport.setViewedComponent(mainContent, true);
        viewport.onVisibleAreaChanged = [this] (juce::Rectangle<int> visible)
        {
            if (mainContent != nullptr)
                mainContent->setVisibleViewportArea(visible);
        };
        addAndMakeVisible(viewport);

        footer.setText(juce::String::fromUTF8(
            "NAVALHA · Glerm Soares, 2009 · NAVALHA 2 · upgrade por "
            "Lúcio Araújo, 2026 · PD reference v0.28.1 · JUCE v"
            JUCE_APPLICATION_VERSION_STRING " · GPL-3.0-or-later"),
            juce::dontSendNotification);
        footer.setJustificationType(juce::Justification::centredLeft);
        footer.setFont(juce::Font(juce::FontOptions(
            "DejaVu Sans Mono", 9.5F, juce::Font::plain)));
        footer.getProperties().set("arcadeFontSize", 9.5);
        footer.setColour(
            juce::Label::textColourId, juce::Colour(Arcade::muted));
        footer.setColour(
            juce::Label::backgroundColourId, juce::Colour(Arcade::background));
        addAndMakeVisible(footer);

        configureJump(edit, "EDIT / PREPARE", 0);
        configureAction(play, "PLAY / PERFORM", [this]
        {
            if (mainContent != nullptr && openPerform)
                openPerform(*mainContent, true);
        });
        configureJump(compose, "COMPOSE / FORM", 610);
        configureJump(mix, "MIX / VOICES", 896);
        configureAction(productionWindow, "TAKES / MASTER", [this]
        {
            if (mainContent != nullptr && openProduction)
                openProduction(*mainContent);
        });
        configureAction(performWindow, "DUAL MONITOR", [this]
        {
            if (mainContent == nullptr || !openPerform)
                return;
            if (!dualMonitorAvailable)
            {
                openPerform(*mainContent, true);
                return;
            }
            dualMonitorRequested = !dualMonitorRequested;
            mainContent->setPrefersDualMonitor(dualMonitorRequested);
            performWindow.setToggleState(
                dualMonitorRequested, juce::dontSendNotification);
            resized();
            openPerform(*mainContent, dualMonitorRequested);
        });
        languagePicker.addItemList(
            {juce::String::fromUTF8("LANG · EN"),
             juce::String::fromUTF8("LANG · PT"),
             juce::String::fromUTF8("LANG · FR"),
             juce::String::fromUTF8("LANG · ES")}, 1);
        languagePicker.setSelectedItemIndex(
            navalha::ui::languageIndex(mainContent->getUiLanguage()),
            juce::dontSendNotification);
        languagePicker.onChange = [this]
        {
            const auto language = static_cast<navalha::ui::Language>(
                juce::jlimit(0, 3, languagePicker.getSelectedItemIndex()));
            mainContent->setUiLanguage(language);
            if (tutorialWindow != nullptr)
                tutorialWindow->setLanguage(language);
            if (appInfoWindow != nullptr)
                appInfoWindow->setLanguage(language);
        };
        languagePicker.getProperties().set("learnKey", "language");
        addAndMakeVisible(languagePicker);

        configureAction(tutorialButton, "TUTORIAL", [this]
        {
            if (tutorialWindow == nullptr)
                tutorialWindow = std::make_unique<TutorialWindow>(
                    mainContent->getUiLanguage());
            tutorialWindow->setLanguage(mainContent->getUiLanguage());
            tutorialWindow->setVisible(true);
            tutorialWindow->toFront(true);
        });
        tutorialButton.setTooltip(
            "Ten-chapter embedded guide based on the PD v0.28.1 tutorial.");

        configureAction(appInfoButton, "ABOUT", [this]
        {
            if (appInfoWindow == nullptr)
                appInfoWindow = std::make_unique<AppInfoWindow>(
                    mainContent->getUiLanguage());
            appInfoWindow->setLanguage(mainContent->getUiLanguage());
            appInfoWindow->setVisible(true);
            appInfoWindow->toFront(true);
        });

        configureAction(learnButton, "LEARN", [this]
        {
            mainContent->setLearningMode(!mainContent->isLearningMode());
            learnButton.setToggleState(
                mainContent->isLearningMode(), juce::dontSendNotification);
        });
        learnButton.setClickingTogglesState(false);
        learnButton.setToggleState(
            mainContent->isLearningMode(), juce::dontSendNotification);

        edit.getProperties().set("learnKey", "waveform");
        play.getProperties().set("learnKey", "play");
        compose.getProperties().set("learnKey", "form");
        mix.getProperties().set("learnKey", "mixer");
        productionWindow.getProperties().set("learnKey", "production");
        performWindow.getProperties().set("learnKey", "dual");
        tutorialButton.getProperties().set("learnKey", "tutorial");
        appInfoButton.getProperties().set("learnKey", "app");
        learnButton.getProperties().set("learnKey", "learn");
        // A Desktop listener receives hover events from every JUCE window,
        // including detached PERFORM, TAKE, MASTER, TUTORIAL and ABOUT views.
        juce::Desktop::getInstance().addGlobalMouseListener(this);
        edit.setToggleState(true, juce::dontSendNotification);
        performWindow.setToggleState(
            dualMonitorRequested, juce::dontSendNotification);

        const juce::Component::SafePointer<NavigationContainer> safeThis(this);
        juce::MessageManager::callAsync([safeThis]
        {
            if (safeThis != nullptr
                && safeThis->mainContent != nullptr
                && safeThis->openPerform)
            {
                safeThis->openPerform(
                    *safeThis->mainContent,
                    safeThis->dualMonitorRequested);
            }
        });
    }

    ~NavigationContainer() override
    {
        viewport.onVisibleAreaChanged = {};
        juce::Desktop::getInstance().removeGlobalMouseListener(this);
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(Arcade::background));
        graphics.setColour(juce::Colour(Arcade::yellow));
        graphics.fillRect(0, 41, getWidth(), 1);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto navigation = area.removeFromTop(42).reduced(5, 4);
        footer.setBounds(area.removeFromBottom(14).reduced(8, 0));
        auto utility = navigation.removeFromRight(
            juce::jmin(350, navigation.getWidth() / 2));
        navigation.removeFromRight(6);
        viewport.setBounds(area);
        const auto visibleWidth = viewport.getMaximumVisibleWidth();
        const auto visibleHeight = viewport.getMaximumVisibleHeight();
        const auto dualFits = dualMonitorRequested
            && visibleWidth >= 1400 && visibleHeight >= 850;
        const auto buttonCount = dualFits ? 5 : 6;
        const auto buttonWidth = navigation.getWidth() / buttonCount;
        edit.setBounds(navigation.removeFromLeft(buttonWidth).reduced(2, 0));
        compose.setBounds(navigation.removeFromLeft(buttonWidth).reduced(2, 0));
        play.setBounds(navigation.removeFromLeft(buttonWidth).reduced(2, 0));
        mix.setVisible(!dualFits);
        if (dualFits)
            mix.setBounds({});
        else
            mix.setBounds(navigation.removeFromLeft(buttonWidth).reduced(2, 0));
        productionWindow.setBounds(
            navigation.removeFromLeft(buttonWidth).reduced(2, 0));
        performWindow.setBounds(navigation.reduced(2, 0));
        const auto utilityButtonWidth = utility.getWidth() / 4;
        languagePicker.setBounds(
            utility.removeFromLeft(utilityButtonWidth).reduced(2, 0));
        tutorialButton.setBounds(
            utility.removeFromLeft(utilityButtonWidth).reduced(2, 0));
        learnButton.setBounds(
            utility.removeFromLeft(utilityButtonWidth).reduced(2, 0));
        appInfoButton.setBounds(
            utility.reduced(2, 0));
        mainContent->setTransform(juce::AffineTransform());
        mainContent->setDualMonitorLayout(dualFits);
        viewport.setScrollBarsShown(!dualFits, false);
        if (auto* content = viewport.getViewedComponent())
            content->setSize(
                juce::jmax(
                    1480, visibleWidth),
                dualFits ? visibleHeight : 1366);
        mainContent->setVisibleViewportArea(viewport.getViewArea());
    }

private:
    void mouseEnter(const juce::MouseEvent& event) override
    {
        if (mainContent == nullptr || !mainContent->isLearningMode())
            return;
        auto* component = event.originalComponent;
        while (component != nullptr && component != this)
        {
            if (component->getProperties().contains("learnKey"))
            {
                mainContent->explainLearnKey(
                    component->getProperties()["learnKey"].toString());
                return;
            }
            component = component->getParentComponent();
        }
    }

    void configureJump(juce::TextButton& button,
                       const juce::String& text,
                       int targetY)
    {
        button.setButtonText(text);
        button.setClickingTogglesState(false);
        auto* selected = &button;
        button.onClick = [this, targetY, selected]
        {
            edit.setToggleState(selected == &edit, juce::dontSendNotification);
            play.setToggleState(selected == &play, juce::dontSendNotification);
            compose.setToggleState(selected == &compose, juce::dontSendNotification);
            mix.setToggleState(selected == &mix, juce::dontSendNotification);
            viewport.setViewPosition(0, targetY);
        };
        addAndMakeVisible(button);
    }

    void configureAction(juce::TextButton& button,
                         const juce::String& text,
                         std::function<void()> action)
    {
        button.setButtonText(text);
        button.onClick = std::move(action);
        addAndMakeVisible(button);
    }

    static bool hasDualMonitorSpace()
    {
        const auto& displayManager =
            juce::Desktop::getInstance().getDisplays();
        if (displayManager.displays.size() < 2)
            return false;
        const auto* primary = displayManager.getPrimaryDisplay();
        if (primary == nullptr)
            return false;
        const auto primaryBounds = primary->userBounds.toNearestInt();
        if (primaryBounds.getWidth() < 1400
            || primaryBounds.getHeight() < 850)
            return false;
        for (const auto& display : displayManager.displays)
        {
            if (&display == primary)
                continue;
            const auto bounds = display.userBounds.toNearestInt();
            if (bounds.getWidth() >= 1100 && bounds.getHeight() >= 650)
                return true;
        }
        return false;
    }

    ArcadeLookAndFeel arcadeLookAndFeel;
    PagedViewport viewport;
    MainComponent* mainContent = nullptr;
    std::function<void(MainComponent&, bool)> openPerform;
    std::function<void(MainComponent&)> openProduction;
    juce::Label footer;
    juce::TextButton edit;
    juce::TextButton play;
    juce::TextButton compose;
    juce::TextButton mix;
    juce::TextButton productionWindow;
    juce::TextButton performWindow;
    juce::ComboBox languagePicker;
    juce::TextButton tutorialButton;
    juce::TextButton appInfoButton;
    juce::TextButton learnButton;
    std::unique_ptr<TutorialWindow> tutorialWindow;
    std::unique_ptr<AppInfoWindow> appInfoWindow;
    bool dualMonitorAvailable = false;
    bool dualMonitorRequested = false;
};

class PerformanceWindow final : public juce::DocumentWindow
{
public:
    PerformanceWindow(MainComponent& main,
                      PerformanceRemoteComponent* preparedContent = nullptr)
        : DocumentWindow("Navalha 2 - PERFORM",
                         juce::Colours::black,
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setIcon(navalhaAppIcon());
        setContentOwned(
            preparedContent != nullptr
                ? preparedContent : new PerformanceRemoteComponent(main),
            true);
        setResizable(true, true);
        setResizeLimits(900, 560, 1920, 1200);
        centreWithSize(1120, 680);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }
};

class ProductionWindow final : public juce::DocumentWindow
{
public:
    explicit ProductionWindow(MainComponent& main)
        : DocumentWindow("Navalha 2 - TAKES / MASTER",
                         juce::Colours::black,
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setIcon(navalhaAppIcon());
        setContentOwned(new ProductionWorkspaceComponent(main), true);
        setResizable(true, true);
        setResizeLimits(980, 660, 2560, 1440);
        if (const auto* display =
                juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            setBounds(display->userBounds.toNearestInt().reduced(18));
        else
            centreWithSize(1800, 920);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }
};

class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow(std::function<void(MainComponent&, bool)> showPerform,
               std::function<void(MainComponent&)> showProduction)
        : DocumentWindow("Navalha 2 - JUCE/C++",
                         juce::Colours::black,
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setIcon(navalhaAppIcon());
        setContentOwned(
            new NavigationContainer(
                std::move(showPerform), std::move(showProduction)),
            true);
        setResizable(true, true);
        if (const auto* display =
                juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            setBounds(display->userBounds.toNearestInt());
        else
            centreWithSize(1600, 1000);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class NavalhaApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return "Navalha 2";
    }

    const juce::String getApplicationVersion() override
    {
        return JUCE_APPLICATION_VERSION_STRING;
    }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(
            [this] (MainComponent& main, bool makeVisible)
            {
                showPerform(main, makeVisible);
            },
            [this] (MainComponent& main) { showProduction(main); });
    }

    void shutdown() override
    {
        performWindow.reset();
        preparedPerformance.reset();
        productionWindow.reset();
        mainWindow.reset();
    }

private:
    void showPerform(MainComponent& main, bool makeVisible)
    {
        if (!makeVisible)
        {
            if (performWindow != nullptr)
            {
                performWindow->setVisible(false);
                return;
            }
            if (performWindow == nullptr && preparedPerformance == nullptr)
            {
                preparedPerformance =
                    std::make_unique<PerformanceRemoteComponent>(main);
                preparedPerformance->setSize(1120, 680);
            }
            return;
        }

        if (performWindow == nullptr)
        {
            performWindow = std::make_unique<PerformanceWindow>(
                main, preparedPerformance.release());
        }
        const auto& displayManager =
            juce::Desktop::getInstance().getDisplays();
        const auto* primary = displayManager.getPrimaryDisplay();
        for (const auto& display : displayManager.displays)
        {
            if (&display != primary)
            {
                performWindow->setBounds(display.userBounds.toNearestInt());
                break;
            }
        }
        performWindow->setVisible(true);
        performWindow->toFront(true);
    }

    void showProduction(MainComponent& main)
    {
        if (productionWindow == nullptr)
            productionWindow = std::make_unique<ProductionWindow>(main);
        const juce::Displays::Display* widestDisplay = nullptr;
        for (const auto& display :
             juce::Desktop::getInstance().getDisplays().displays)
        {
            if (widestDisplay == nullptr
                || display.userBounds.getWidth()
                    > widestDisplay->userBounds.getWidth())
                widestDisplay = &display;
        }
        if (widestDisplay != nullptr)
            productionWindow->setBounds(
                widestDisplay->userBounds.toNearestInt().reduced(18));
        productionWindow->setVisible(true);
        productionWindow->toFront(true);
    }

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<PerformanceWindow> performWindow;
    std::unique_ptr<PerformanceRemoteComponent> preparedPerformance;
    std::unique_ptr<ProductionWindow> productionWindow;
};
}

#if JUCE_LINUX
JUCE_CREATE_APPLICATION_DEFINE(NavalhaApplication)

int main(int argumentCount, char* arguments[])
{
    auto* display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        std::fputs(
            "Navalha 2 JUCE requires an accessible X11 display.\n", stderr);
        return 3;
    }
    XCloseDisplay(display);
    juce::JUCEApplicationBase::createInstance = &juce_CreateApplication;
    return juce::JUCEApplicationBase::main(
        argumentCount, const_cast<const char**>(arguments));
}
#else
START_JUCE_APPLICATION(NavalhaApplication)
#endif
