#pragma once

#include <cstddef>
#include <vector>

#include "core/SessionModel.h"
#include "core/StereoMixer.h"

namespace navalha
{
class StereoAudioBuffer
{
public:
    StereoAudioBuffer(double sampleRate,
                      std::vector<float> leftChannel,
                      std::vector<float> rightChannel);

    [[nodiscard]] double sampleRate() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] StereoSample interpolated(double position) const noexcept;

private:
    double rate;
    std::vector<float> left;
    std::vector<float> right;
};

class SlicePlayer
{
public:
    void prepare(double newOutputSampleRate);
    void setBuffer(const StereoAudioBuffer* newBuffer) noexcept;
    void trigger(Slice slice,
                 bool reverse = false,
                 double playbackRate = 1.0,
                 double attackSeconds = -1.0,
                 double releaseSeconds = -1.0);
    [[nodiscard]] bool tryTrigger(Slice slice,
                                  bool reverse = false,
                                  double playbackRate = 1.0,
                                  double attackSeconds = -1.0,
                                  double releaseSeconds = -1.0) noexcept;
    void stop() noexcept;

    [[nodiscard]] StereoSample process() noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] double normalizedPosition() const noexcept;

private:
    [[nodiscard]] float envelope() const noexcept;

    const StereoAudioBuffer* buffer = nullptr;
    double outputSampleRate = 44100.0;
    double position = 0.0;
    double increment = 1.0;
    std::size_t renderedSamples = 0;
    std::size_t totalSamples = 0;
    std::size_t attackSamples = 1;
    std::size_t releaseSamples = 1;
    std::size_t stopFadeRemaining = 0;
    std::size_t stopFadeLength = 1;
    bool reversePlayback = false;
    bool stopping = false;
    bool playing = false;
};
}
