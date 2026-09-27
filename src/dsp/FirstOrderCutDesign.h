#pragma once

#include "BiquadCoefficients.h"

//==============================================================================
/** First-order lowpass and highpass sections, used for odd cut orders.

    Own derivation, not a published formula: pole by impulse invariance
    (a1 = -exp(-wc), after Vicanek 2016 section 3.2), numerator from unity gain
    in the passband (DC for lowpass; a zero at DC for highpass) and
    |H(wc)| = 1/sqrt(2) at the cutoff. Returned with b2 = a2 = 0.
*/
class FirstOrderCutDesign
{
public:
    static BiquadCoefficients designLowpass  (double cutoffHz, double sampleRate) noexcept;
    static BiquadCoefficients designHighpass (double cutoffHz, double sampleRate) noexcept;
};
