#pragma once

#include "BandSettings.h"

#include <array>
#include <complex>
#include <span>

//==============================================================================
/** The chain as a 2x2 complex transfer matrix per frequency, [L' R'] = M [L R]
    (decision 2026-09-28), used for Auto Gain and the summed curves.

      Stereo: H I          Left: diag (H, 1)          Right: diag (1, H)
      Mid:  T^-1 diag (H, 1) T        Side: T^-1 diag (1, H) T
      with T = [[1/2, 1/2], [1/2, -1/2]] (L/R to M/S) and T^-1 = [[1, 1], [1, -1]].

    Bands multiply in chain order (band 1 first). For uncorrelated, equal-level L
    and R inputs, the output power relative to the input is ||M||_F^2 / 2.
*/
class StereoTransfer
{
public:
    using Complex = std::complex<double>;
    using Matrix = std::array<std::array<Complex, 2>, 2>;   // [row][column]

    static Matrix identity() noexcept;
    static Matrix multiply (const Matrix& a, const Matrix& b) noexcept;   // a b

    /** A band's matrix given its (complex) response H on the channel it acts on. */
    static Matrix forBand (ChannelMode mode, Complex h) noexcept;

    /** The chain of the given bands at one frequency: bands that are not active are skipped,
        cuts too when includeCuts is false. */
    static Matrix chain (std::span<const BandSettings> bands, double frequencyHz, double sampleRate, bool includeCuts);

    /** Output power over input power for uncorrelated, equal-level L and R: ||M||^2 / 2. */
    static double powerGain (const Matrix& m) noexcept;

    /** The same matrix in the mid/side basis: T M T^-1. */
    static Matrix toMidSide (const Matrix& m) noexcept;
};
