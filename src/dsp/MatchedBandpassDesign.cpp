#include "MatchedBandpassDesign.h"

#include "MatchedDesignMath.h"

#include <algorithm>
#include <cmath>

BiquadCoefficients MatchedBandpassDesign::design (double centreHz, double q, double sampleRate) noexcept
{
    using namespace MatchedDesignMath;

    const auto w0 = omega (centreHz, sampleRate);
    const auto poles = impulseInvariantPoles (w0, 1.0 / (2.0 * q));   // eq. 12
    const auto A = denominatorTerms (poles);
    const auto p = phi (w0);

    // Zero at DC (B0 = 0), maximum and unity gain at w0: eqs. 38-40.
    // Eq. 40 as printed subtracts nearly equal terms (R1 - R2 phi1) at low w0 and high Q,
    // which turns B1 negative in double precision (e.g. 20 Hz, Q 18 at 192 kHz). With
    // phi0 = 1 - phi1 it simplifies exactly to the forms below:
    //   R1 - R2 phi1 = A0 + 4 A2 phi1^2   ->  B2 = A0 / (4 phi1^2) + A2
    //   B1 = R2 + 4 (phi1 - phi0) B2       ->  B1 = A1 - A0 phi0^2 / phi1^2
    const auto ratio = p.phi0 / p.phi1;
    const auto B2 = A.t0 / (4.0 * p.phi1 * p.phi1) + A.t2;
    const auto B1 = std::max (A.t1 - A.t0 * ratio * ratio, 0.0);

    // Eq. 41.
    BiquadCoefficients c;
    c.a1 = poles.a1;
    c.a2 = poles.a2;
    c.b1 = -0.5 * std::sqrt (B1);
    c.b0 = 0.5 * (std::sqrt (B2 + c.b1 * c.b1) - c.b1);
    c.b2 = -c.b0 - c.b1;
    return c;
}

double MatchedBandpassDesign::analogMagnitudeDb (double frequencyHz, double centreHz, double q) noexcept
{
    // H(s) = s w0/Q / (w0^2 + s w0/Q + s^2)  ->  |H|^2 = (x/Q)^2 / ((1 - x^2)^2 + (x/Q)^2)
    const auto x = frequencyHz / centreHz;
    const auto num = (x / q) * (x / q);
    return 10.0 * std::log10 (num / ((1.0 - x * x) * (1.0 - x * x) + num));
}
