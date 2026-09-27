#pragma once

#include "BiquadCoefficients.h"

//==============================================================================
/** Second-order all-pass.

    Own derivation, not a published formula: poles by impulse invariance
    (Vicanek 2016, eq. 12) and the numerator as the mirrored denominator,
    which makes |H| = 1 at every frequency.
*/
class MatchedAllpassDesign
{
public:
    static BiquadCoefficients design (double centreHz, double q, double sampleRate) noexcept;
};
