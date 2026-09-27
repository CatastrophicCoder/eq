#pragma once

#include "BiquadCoefficients.h"

//==============================================================================
/** Peaking (bell) EQ coefficients matched to the analog prototype.

    Source: Martin Vicanek, "Matched Second Order Digital Filters", 14 Feb 2016,
    section 4.4 (https://www.vicanek.de/articles/BiquadFits.pdf).
*/
class MatchedPeakingDesign
{
public:
    /** Designs the digital filter. Frequencies in Hz, gain in dB. */
    static BiquadCoefficients design (double centreHz, double gainDb, double q, double sampleRate) noexcept;

    /** Magnitude in dB of the analog prototype (Vicanek eq. 42) at the given frequency. */
    static double analogMagnitudeDb (double frequencyHz, double centreHz, double gainDb, double q) noexcept;
};
