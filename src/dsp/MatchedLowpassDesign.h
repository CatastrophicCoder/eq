#pragma once

#include "BiquadCoefficients.h"

//==============================================================================
/** Second-order lowpass matched to the analog prototype: unity DC gain and
    |H| = Q at the cutoff, single zero (b2 = 0).

    Source: Vicanek, "Matched Second Order Digital Filters" (2016), section 4.1, eqs. 30-34.
*/
class MatchedLowpassDesign
{
public:
    static BiquadCoefficients design (double cutoffHz, double q, double sampleRate) noexcept;

    /** Analog prototype, eq. 30. */
    static double analogMagnitudeDb (double frequencyHz, double cutoffHz, double q) noexcept;
};
