#pragma once

#include <optional>
#include <span>
#include <vector>

//==============================================================================
/** Finds peaks in a spectrum for peak pick (M9c, decisions 2026-09-29).

    A peak is a local maximum at least minProminenceDb above the lowest level on each
    side before the spectrum rises higher again, looking at most searchOctaves away.
    Its Q is f0 / (-3 dB width), the width interpolated on a log-frequency axis.
*/
class PeakFinder
{
public:
    static constexpr double minProminenceDb = 3.0;
    static constexpr double searchOctaves = 1.0;
    static constexpr double nearOctaves = 0.5;

    struct Peak
    {
        int point = 0;
        double frequencyHz = 0.0, levelDb = 0.0, prominenceDb = 0.0, q = 1.0;
    };

    static std::vector<Peak> find (std::span<const double> frequenciesHz, std::span<const double> levelsDb);

    /** The most prominent peak within nearOctaves of a frequency. */
    static std::optional<Peak> nearest (const std::vector<Peak>& peaks, double frequencyHz);
};
