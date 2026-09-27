#pragma once

#include "BiquadCoefficients.h"

//==============================================================================
/** Notch (band reject).

    Own derivation, not a published formula: poles by impulse invariance
    (Vicanek 2016, eq. 12), zeros exactly on the unit circle at w0, and the
    numerator scaled for unity gain at DC.
*/
class MatchedNotchDesign
{
public:
    static BiquadCoefficients design (double centreHz, double q, double sampleRate) noexcept;

    /** Analog prototype H(s) = (s^2 + w0^2) / (s^2 + s w0/Q + w0^2). */
    static double analogMagnitudeDb (double frequencyHz, double centreHz, double q) noexcept;
};
