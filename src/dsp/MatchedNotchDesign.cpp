#include "MatchedNotchDesign.h"

#include "MatchedDesignMath.h"

#include <cmath>

BiquadCoefficients MatchedNotchDesign::design (double centreHz, double q, double sampleRate) noexcept
{
    using namespace MatchedDesignMath;

    const auto w0 = omega (centreHz, sampleRate);
    const auto poles = impulseInvariantPoles (w0, 1.0 / (2.0 * q));   // Vicanek 2016 eq. 12

    // Zeros at exp(+-j w0): numerator k (1 - 2 cos(w0) z^-1 + z^-2), with k chosen
    // so that H(1) = 1, i.e. k (2 - 2 cos w0) = 1 + a1 + a2.
    const auto cosW0 = std::cos (w0);
    const auto k = (1.0 + poles.a1 + poles.a2) / (2.0 - 2.0 * cosW0);

    BiquadCoefficients c;
    c.a1 = poles.a1;
    c.a2 = poles.a2;
    c.b0 = k;
    c.b1 = -2.0 * cosW0 * k;
    c.b2 = k;
    return c;
}

double MatchedNotchDesign::analogMagnitudeDb (double frequencyHz, double centreHz, double q) noexcept
{
    // |H|^2 = (1 - x^2)^2 / ((1 - x^2)^2 + (x/Q)^2); -inf at x = 1
    const auto x = frequencyHz / centreHz;
    const auto num = (1.0 - x * x) * (1.0 - x * x);
    return 10.0 * std::log10 (num / (num + (x / q) * (x / q)));
}
