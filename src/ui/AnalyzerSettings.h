#pragma once

#include <array>

//==============================================================================
/** Analyzer options (decisions 2026-09-28). Stored in the session as state
    properties; indices are saved, so never reorder.
*/
namespace AnalyzerSettings
{
    enum class Mode { off, pre, post, prePost };
    inline constexpr std::array<const char*, 4> modeNames { "Off", "Pre", "Post", "Pre+Post" };

    /** FFT sizes: Low 2048, Medium 4096, High 8192, Max 16384. */
    inline constexpr std::array<int, 4> fftOrders { 11, 12, 13, 14 };
    inline constexpr std::array<const char*, 4> resolutionNames { "2048", "4096", "8192", "16384" };   // FFT points

    /** Release (fall) rates in dB per second. */
    inline constexpr std::array<double, 3> releaseDbPerSecond { 10.0, 25.0, 60.0 };
    inline constexpr std::array<const char*, 3> speedNames { "Slow", "Medium", "Fast" };

    inline constexpr std::array<double, 3> ranges { 60.0, 90.0, 120.0 };

    /** Display tilt: +4.5 dB per octave around 1 kHz. */
    inline constexpr double tiltDbPerOctave = 4.5;
    inline constexpr double tiltPivotHz = 1000.0;

    struct Values
    {
        int mode = static_cast<int> (Mode::prePost);
        int resolution = 1;   // Medium
        int speed = 1;        // Medium
        int range = 1;        // 90 dB
    };
}
