#include "MatchedPeakingDesign.h"

#include <cmath>
#include <numbers>

BiquadCoefficients MatchedPeakingDesign::design (double centreHz, double gainDb, double q, double sampleRate) noexcept
{
    const auto G  = std::pow (10.0, gainDb / 20.0);
    const auto G2 = G * G;
    const auto w0 = 2.0 * std::numbers::pi * centreHz / sampleRate;   // radians per sample

    BiquadCoefficients c;

    // Poles by impulse invariance, eq. 12. The eq. 42 denominator
    // s^2 + s w0 / (sqrt(G) Q) + w0^2 gives the damping 2 zeta = 1 / (sqrt(G) Q).
    const auto zeta = 1.0 / (2.0 * std::sqrt (G) * q);
    const auto decay = std::exp (-zeta * w0);

    c.a1 = zeta <= 1.0 ? -2.0 * decay * std::cos  (std::sqrt (1.0 - zeta * zeta) * w0)
                       : -2.0 * decay * std::cosh (std::sqrt (zeta * zeta - 1.0) * w0);
    c.a2 = decay * decay;

    // Squared-magnitude form, eqs. 25-27, evaluated at w0.
    const auto A0 = (1.0 + c.a1 + c.a2) * (1.0 + c.a1 + c.a2);
    const auto A1 = (1.0 - c.a1 + c.a2) * (1.0 - c.a1 + c.a2);
    const auto A2 = -4.0 * c.a2;

    const auto sinHalf = std::sin (w0 / 2.0);
    const auto phi1 = sinHalf * sinHalf;
    const auto phi0 = 1.0 - phi1;
    const auto phi2 = 4.0 * phi0 * phi1;

    // Unity gain at DC, |H| = G at w0, and an extremum at w0: eqs. 43-45.
    const auto R1 = (A0 * phi0 + A1 * phi1 + A2 * phi2) * G2;
    const auto R2 = (-A0 + A1 + 4.0 * (phi0 - phi1) * A2) * G2;

    const auto B0 = A0;
    const auto B2 = (R1 - R2 * phi1 - B0) / (4.0 * phi1 * phi1);
    const auto B1 = R2 + B0 + 4.0 * (phi1 - phi0) * B2;

    // Minimum-phase numerator from B0, B1, B2: eq. 29.
    const auto sqrtB0 = std::sqrt (B0);
    const auto sqrtB1 = std::sqrt (B1);
    const auto W = 0.5 * (sqrtB0 + sqrtB1);

    c.b0 = 0.5 * (W + std::sqrt (W * W + B2));
    c.b1 = 0.5 * (sqrtB0 - sqrtB1);
    c.b2 = -B2 / (4.0 * c.b0);

    return c;
}

double MatchedPeakingDesign::analogMagnitudeDb (double frequencyHz, double centreHz, double gainDb, double q) noexcept
{
    // Vicanek eq. 42:
    //   H(s) = (w0^2 + s w0 sqrt(G)/Q + s^2) / (w0^2 + s w0 / (sqrt(G) Q) + s^2)
    // evaluated at s = jw. Only the ratio w/w0 matters, so work in Hz.
    const auto sqrtG = std::sqrt (std::pow (10.0, gainDb / 20.0));
    const auto w  = frequencyHz;
    const auto w0 = centreHz;

    const auto real = w0 * w0 - w * w;
    const auto numImag = w * w0 * sqrtG / q;
    const auto denImag = w * w0 / (sqrtG * q);

    const auto magSquared = (real * real + numImag * numImag) / (real * real + denImag * denImag);
    return 10.0 * std::log10 (magSquared);
}
