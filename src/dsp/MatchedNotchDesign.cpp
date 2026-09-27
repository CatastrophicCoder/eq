#include "MatchedNotchDesign.h"

#include <cmath>

BiquadCoefficients MatchedNotchDesign::design (double centreHz, double q, double sampleRate) noexcept
{
    // Not implemented yet.
    (void) centreHz; (void) q; (void) sampleRate;
    return {};
}

double MatchedNotchDesign::analogMagnitudeDb (double frequencyHz, double centreHz, double q) noexcept
{
    // |H|^2 = (1 - x^2)^2 / ((1 - x^2)^2 + (x/Q)^2); -inf at x = 1
    const auto x = frequencyHz / centreHz;
    const auto num = (1.0 - x * x) * (1.0 - x * x);
    return 10.0 * std::log10 (num / (num + (x / q) * (x / q)));
}
