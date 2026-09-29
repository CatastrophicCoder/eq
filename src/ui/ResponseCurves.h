#pragma once

#include "dsp/BandSettings.h"

#include <array>
#include <span>

//==============================================================================
/** Magnitude curves for the display: each band and their sum, at numPoints
    log-spaced frequencies from 20 Hz to 20 kHz, from BandDesign.

    Dynamic bands (M7, decision 2026-09-28: live gain plus the range) are drawn at
    static gain + live change; their static and static + range curves are kept too.

    update() recomputes only when a band's settings or the sample rate changed, or
    a dynamic band's live change moved by liveGainStepDb or more.
*/
class ResponseCurves
{
public:
    static constexpr int numPoints = 512;
    static constexpr int numBands = 16;
    static constexpr double liveGainStepDb = 0.05;

    ResponseCurves();

    /** Returns true if the curves were recomputed. */
    bool update (std::span<const BandSettings> bands, double sampleRate);

    /** Same, with each band's live dynamic gain change in dB (M7). */
    bool update (std::span<const BandSettings> bands, double sampleRate, std::span<const double> liveGainDb);

    /** Same, with each spectral band's live per-slice gain change at the curve's points (M9g). */
    using SliceGains = std::array<std::array<double, 512>, 16>;
    bool update (std::span<const BandSettings> bands, double sampleRate, std::span<const double> liveGainDb, const SliceGains& spectralDb);

    int getNumRecomputes() const noexcept { return recomputes; }

    double frequency (int point) const noexcept   { return frequencies[static_cast<size_t> (point)]; }
    /** In use and enabled: part of the sum. */
    bool isBandActive (int band) const noexcept   { return active[static_cast<size_t> (band)]; }
    /** In use (enabled or not): has a curve to draw. */
    bool isBandShown (int band) const noexcept    { return shown[static_cast<size_t> (band)]; }
    double bandDb (int band, int point) const noexcept;

    /** Active, dynamic and of a type that can be dynamic (M7): has static and range curves too. */
    bool isBandDynamic (int band) const noexcept { return dynamic[static_cast<size_t> (band)]; }
    /** A dynamic band's curve at its static gain, and at static gain + range. */
    double staticBandDb (int band, int point) const noexcept;
    double rangeBandDb (int band, int point) const noexcept;
    /** Which sums are shown: one (all Stereo), L and R, or M and S (decision 2026-09-28). */
    enum class SumLayout { single, leftRight, midSide };
    SumLayout getSumLayout() const noexcept { return layout; }

    /** The only sum (single), or L / M. */
    double sumDb (int point) const noexcept       { return sum[static_cast<size_t> (point)]; }
    /** R / S; equals sumDb in the single layout. */
    double secondSumDb (int point) const noexcept { return secondSum[static_cast<size_t> (point)]; }

private:
    std::array<double, numPoints> frequencies {};
    std::array<std::array<double, numPoints>, numBands> curves {}, staticCurves {}, rangeCurves {};
    std::array<double, numBands> lastLive {};
    std::array<double, numPoints> sum {}, secondSum {};
    SumLayout layout = SumLayout::single;
    std::array<bool, numBands> active {};
    std::array<bool, numBands> shown {};
    std::array<bool, numBands> dynamic {};

    std::array<BandSettings, numBands> lastBands {};
    double lastSampleRate = 0.0;
    bool hasComputed = false;
    int recomputes = 0;
};
