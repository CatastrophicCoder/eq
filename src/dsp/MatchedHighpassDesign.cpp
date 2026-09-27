#include "MatchedHighpassDesign.h"

#include "MatchedDesignMath.h"

#include <cmath>

BiquadCoefficients MatchedHighpassDesign::design (double cutoffHz, double q, double sampleRate) noexcept
{
    using namespace MatchedDesignMath;

    const auto w0 = omega (cutoffHz, sampleRate);
    const auto poles = impulseInvariantPoles (w0, 1.0 / (2.0 * q));   // eq. 12
    const auto p = phi (w0);

    // Double zero at DC (b1 = -2 b0, b2 = b0) and |H(w0)| = Q: eq. 36.
    BiquadCoefficients c;
    c.a1 = poles.a1;
    c.a2 = poles.a2;
    c.b0 = std::sqrt (evaluate (denominatorTerms (poles), p)) / (4.0 * p.phi1) * q;
    c.b1 = -2.0 * c.b0;
    c.b2 = c.b0;
    return c;
}

double MatchedHighpassDesign::analogMagnitudeDb (double frequencyHz, double cutoffHz, double q) noexcept
{
    // H(s) = s^2 / (w0^2 + s w0/Q + s^2)  ->  |H|^2 = x^4 / ((1 - x^2)^2 + (x/Q)^2)
    const auto x = frequencyHz / cutoffHz;
    return 10.0 * std::log10 (x * x * x * x / ((1.0 - x * x) * (1.0 - x * x) + (x / q) * (x / q)));
}
