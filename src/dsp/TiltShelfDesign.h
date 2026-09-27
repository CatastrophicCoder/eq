#pragma once

#include "BiquadCoefficients.h"

//==============================================================================
/** Tilt shelf: a matched one-pole high shelf (MatchedOnePoleShelfDesign) scaled
    so the response runs from -gain/2 at DC to +gain/2 at high frequencies,
    crossing 0 dB at the corner.
*/
class TiltShelfDesign
{
public:
    static BiquadCoefficients design (double cornerHz, double gainDb, double sampleRate) noexcept;

    static double analogMagnitudeDb (double frequencyHz, double cornerHz, double gainDb) noexcept;
};
