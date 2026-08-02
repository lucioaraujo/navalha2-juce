#include "core/WaveformPeaks.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace navalha
{
std::vector<WaveformPeak> buildWaveformPeaks(const StereoAudioBuffer& buffer,
                                             std::size_t requestedBins)
{
    if (requestedBins == 0 || requestedBins > maxWaveformBins)
        throw std::out_of_range("Waveform bin count must be between 1 and 8192");

    const auto binCount = std::min(requestedBins, buffer.size());
    std::vector<WaveformPeak> peaks(binCount);
    for (std::size_t bin = 0; bin < binCount; ++bin)
    {
        const auto begin = bin * buffer.size() / binCount;
        const auto end = std::max(begin + 1, (bin + 1) * buffer.size() / binCount);
        auto& peak = peaks[bin];
        peak.minimumLeft = peak.minimumRight = std::numeric_limits<float>::max();
        peak.maximumLeft = peak.maximumRight = std::numeric_limits<float>::lowest();

        for (std::size_t sampleIndex = begin; sampleIndex < end; ++sampleIndex)
        {
            const auto sample = buffer.interpolated(static_cast<double>(sampleIndex));
            peak.minimumLeft = std::min(peak.minimumLeft, sample.left);
            peak.maximumLeft = std::max(peak.maximumLeft, sample.left);
            peak.minimumRight = std::min(peak.minimumRight, sample.right);
            peak.maximumRight = std::max(peak.maximumRight, sample.right);
        }
    }
    return peaks;
}
}
