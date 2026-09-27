#include "MatchedAllpassDesign.h"

#include "MatchedDesignMath.h"

BiquadCoefficients MatchedAllpassDesign::design (double centreHz, double q, double sampleRate) noexcept
{
    using namespace MatchedDesignMath;

    const auto poles = impulseInvariantPoles (omega (centreHz, sampleRate), 1.0 / (2.0 * q));   // Vicanek 2016 eq. 12

    // Mirrored numerator (a2 + a1 z^-1 + z^-2): |H(e^jw)| = 1 for every w.
    BiquadCoefficients c;
    c.a1 = poles.a1;
    c.a2 = poles.a2;
    c.b0 = poles.a2;
    c.b1 = poles.a1;
    c.b2 = 1.0;
    return c;
}
