#pragma once

#include "dsp/BandSettings.h"

#include <array>
#include <span>

//==============================================================================
/** Magnitude curves for the display: each band and their sum, at numPoints
    log-spaced frequencies from 20 Hz to 20 kHz, from BandDesign.

    update() recomputes only when a band's settings or the sample rate changed.
*/
class ResponseCurves
{
public:
    static constexpr int numPoints = 512;
    static constexpr int numBands = 16;

    ResponseCurves();

    /** Returns true if the curves were recomputed. */
    bool update (std::span<const BandSettings> bands, double sampleRate);

    int getNumRecomputes() const noexcept { return recomputes; }

    double frequency (int point) const noexcept   { return frequencies[static_cast<size_t> (point)]; }
    /** In use and enabled: part of the sum. */
    bool isBandActive (int band) const noexcept   { return active[static_cast<size_t> (band)]; }
    /** In use (enabled or not): has a curve to draw. */
    bool isBandShown (int band) const noexcept    { return active[static_cast<size_t> (band)]; }   // Not implemented yet.
    double bandDb (int band, int point) const noexcept;
    double sumDb (int point) const noexcept       { return sum[static_cast<size_t> (point)]; }

private:
    std::array<double, numPoints> frequencies {};
    std::array<std::array<double, numPoints>, numBands> curves {};
    std::array<double, numPoints> sum {};
    std::array<bool, numBands> active {};

    std::array<BandSettings, numBands> lastBands {};
    double lastSampleRate = 0.0;
    bool hasComputed = false;
    int recomputes = 0;
};
