#pragma once

#include "BiquadCoefficients.h"

//==============================================================================
/** Second-order Butterworth shelves matched to the analog prototype.

    Source: Vicanek, "Matched Two-Pole Digital Shelving Filters" (23 Mar 2024,
    revised 3 May and 10 Dec 2025), section 3, eqs. 1-15; low shelf per section 4.
    fc is the transition midpoint, where the gain is half the shelf gain in dB.
*/
class MatchedShelfDesign
{
public:
    static BiquadCoefficients designHigh (double cornerHz, double gainDb, double sampleRate) noexcept;
    static BiquadCoefficients designLow  (double cornerHz, double gainDb, double sampleRate) noexcept;

    /** Analog prototype, eq. 1 (and its low-shelf mirror). */
    static double analogHighMagnitudeDb (double frequencyHz, double cornerHz, double gainDb) noexcept;
    static double analogLowMagnitudeDb  (double frequencyHz, double cornerHz, double gainDb) noexcept;
};
