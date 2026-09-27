#include "MatchedLowpassDesign.h"

#include "MatchedDesignMath.h"

#include <cmath>

BiquadCoefficients MatchedLowpassDesign::design (double cutoffHz, double q, double sampleRate) noexcept
{
    using namespace MatchedDesignMath;

    const auto w0 = omega (cutoffHz, sampleRate);
    const auto poles = impulseInvariantPoles (w0, 1.0 / (2.0 * q));   // eq. 12
    const auto A = denominatorTerms (poles);
    const auto p = phi (w0);

    // Unity DC gain and |H(w0)| = Q with b2 = 0: eqs. 31-33.
    const auto B0 = A.t0;
    const auto R1 = evaluate (A, p) * q * q;
    const auto B1 = (R1 - B0 * p.phi0) / p.phi1;

    BiquadCoefficients c;
    c.a1 = poles.a1;
    c.a2 = poles.a2;
    c.b0 = 0.5 * (std::sqrt (B0) + std::sqrt (B1));
    c.b1 = std::sqrt (B0) - c.b0;
    c.b2 = 0.0;
    return c;
}

double MatchedLowpassDesign::analogMagnitudeDb (double frequencyHz, double cutoffHz, double q) noexcept
{
    // H(s) = w0^2 / (w0^2 + s w0/Q + s^2), s = jw  ->  |H|^2 = 1 / ((1 - x^2)^2 + (x/Q)^2), x = f/fc
    const auto x = frequencyHz / cutoffHz;
    return -10.0 * std::log10 ((1.0 - x * x) * (1.0 - x * x) + (x / q) * (x / q));
}
