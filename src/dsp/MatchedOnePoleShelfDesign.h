#pragma once

#include "BiquadCoefficients.h"

//==============================================================================
/** First-order high shelf matched to the analog prototype.

    Source: Vicanek, "Matched One-Pole Digital Shelving Filters" (7 Sep 2019,
    revised 24 Sep 2019), eqs. 1-12, with the matching point f_m = 0.9 of
    Nyquist (eq. 12). Returned with b2 = a2 = 0.
*/
class MatchedOnePoleShelfDesign
{
public:
    static BiquadCoefficients designHigh (double cornerHz, double gainDb, double sampleRate) noexcept;

    /** Analog prototype, eq. 1. */
    static double analogHighMagnitudeDb (double frequencyHz, double cornerHz, double gainDb) noexcept;
};
