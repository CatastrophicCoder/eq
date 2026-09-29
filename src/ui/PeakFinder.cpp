#include "PeakFinder.h"

#include <algorithm>
#include <cmath>

std::vector<PeakFinder::Peak> PeakFinder::find (std::span<const double> frequenciesHz, std::span<const double> levelsDb)
{
    const auto n = static_cast<int> (std::min (frequenciesHz.size(), levelsDb.size()));
    std::vector<Peak> peaks;
    if (n < 3)
        return peaks;

    auto octave = [&] (int i) { return std::log2 (frequenciesHz[static_cast<size_t> (i)]); };
    auto level = [&] (int i) { return levelsDb[static_cast<size_t> (i)]; };

    for (int i = 1; i < n - 1; ++i)
    {
        if (! (level (i) > level (i - 1) && level (i) >= level (i + 1)))
            continue;

        // Prominence: the lowest level on each side before the spectrum rises above this peak.
        auto sideMinimum = [&] (int step)
        {
            auto lowest = level (i);
            for (int j = i + step; j >= 0 && j < n && std::abs (octave (j) - octave (i)) <= searchOctaves; j += step)
            {
                if (level (j) > level (i))
                    break;
                lowest = std::min (lowest, level (j));
            }
            return lowest;
        };

        const auto prominence = level (i) - std::max (sideMinimum (-1), sideMinimum (1));
        if (prominence < minProminenceDb)
            continue;

        // -3 dB points, interpolated in log frequency; the search limit if the level never drops that far.
        const auto target = level (i) - 3.0;
        auto halfPower = [&] (int step)
        {
            for (int j = i + step; j >= 0 && j < n && std::abs (octave (j) - octave (i)) <= searchOctaves; j += step)
                if (level (j) < target)
                {
                    const auto inner = j - step;
                    const auto t = (level (inner) - target) / (level (inner) - level (j));
                    return octave (inner) + t * (octave (j) - octave (inner));
                }
            return octave (i) + step * searchOctaves;
        };

        const auto f0 = frequenciesHz[static_cast<size_t> (i)];
        const auto width = std::exp2 (halfPower (1)) - std::exp2 (halfPower (-1));
        peaks.push_back ({ i, f0, level (i), prominence, f0 / width });
    }

    return peaks;
}

std::optional<PeakFinder::Peak> PeakFinder::nearest (const std::vector<Peak>& peaks, double frequencyHz)
{
    std::optional<Peak> best;
    for (const auto& p : peaks)
        if (std::abs (std::log2 (p.frequencyHz / frequencyHz)) <= nearOctaves
            && (! best.has_value() || p.prominenceDb > best->prominenceDb))
            best = p;
    return best;
}
