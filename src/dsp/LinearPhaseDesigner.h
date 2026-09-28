#pragma once

#include "BandSettings.h"

#include <array>
#include <span>
#include <vector>

//==============================================================================
/** Turns the EQ curve into linear-phase FIR filters (M8).

    Frequency sampling: the zero-phase response of the chain (each band's
    magnitude, combined as the 2x2 stereo matrix of StereoTransfer, so each
    entry is real) is sampled on an FFT grid oversampled times finer than the
    filter length, inverse transformed, centred and cut to numTaps with a
    4-term Blackman-Harris window (Harris, "On the Use of Windows for Harmonic
    Analysis with the Discrete Fourier Transform", Proc. IEEE 66(1), 1978).
    Tap 0 is zero, so the filter is symmetric about tap numTaps / 2 and delays
    by exactly latencyFor (numTaps) samples.

    Only active, non-dynamic bands are included: dynamic bands run as normal
    filters after the FIR (decision 2026-09-29).

    Background thread only: design() allocates and runs large FFTs.
*/
class LinearPhaseDesigner
{
public:
    /** Latency menu (decision 2026-09-29): the same tap counts at every sample rate. */
    static constexpr std::array<int, 3> tapCounts { 8192, 16384, 32768 };
    static constexpr int oversampling = 4;

    /** Resolution: features narrower than lowestAccurateBins * fs / numTaps (a band's bandwidth
        f0 / Q, or its frequency) are smoothed by the window beyond the stated bounds. */
    static constexpr double lowestAccurateBins = 32.0;

    static double resolutionHz (int numTaps, double sampleRate) noexcept { return lowestAccurateBins * sampleRate / numTaps; }

    static constexpr int latencyFor (int numTaps) noexcept { return numTaps / 2; }

    /** [output][input] filters: leftFromRight and rightFromLeft are only filled (and used)
        when a Mid or Side band is active. */
    struct Result
    {
        int numTaps = 0;
        bool hasCrossTerms = false;
        std::vector<float> leftFromLeft, leftFromRight, rightFromLeft, rightFromRight;
    };

    void design (std::span<const BandSettings> bands, double sampleRate, int numTaps, Result& result);
};
