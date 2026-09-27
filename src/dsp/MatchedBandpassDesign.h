#pragma once

#include "BiquadCoefficients.h"

//==============================================================================
/** Band pass with unity gain at the centre, matched to the analog prototype:
    single zero at DC, maximum and unity gain at f0.

    Source: Vicanek, "Matched Second Order Digital Filters" (2016), section 4.3, eqs. 37-41.
*/
class MatchedBandpassDesign
{
public:
    static BiquadCoefficients design (double centreHz, double q, double sampleRate) noexcept;

    /** Analog prototype, eq. 37. */
    static double analogMagnitudeDb (double frequencyHz, double centreHz, double q) noexcept;
};
