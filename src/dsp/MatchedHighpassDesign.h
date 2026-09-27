#pragma once

#include "BiquadCoefficients.h"

//==============================================================================
/** Second-order highpass matched to the analog prototype: double zero at DC and
    |H| = Q at the cutoff.

    Source: Vicanek, "Matched Second Order Digital Filters" (2016), section 4.2, eqs. 35-36.
*/
class MatchedHighpassDesign
{
public:
    static BiquadCoefficients design (double cutoffHz, double q, double sampleRate) noexcept;

    /** Analog prototype, eq. 35. */
    static double analogMagnitudeDb (double frequencyHz, double cutoffHz, double q) noexcept;
};
