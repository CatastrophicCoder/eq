#include "MatchedHighpassDesign.h"

#include <cmath>

BiquadCoefficients MatchedHighpassDesign::design (double cutoffHz, double q, double sampleRate) noexcept
{
    // Not implemented yet.
    (void) cutoffHz; (void) q; (void) sampleRate;
    return {};
}

double MatchedHighpassDesign::analogMagnitudeDb (double frequencyHz, double cutoffHz, double q) noexcept
{
    // H(s) = s^2 / (w0^2 + s w0/Q + s^2)  ->  |H|^2 = x^4 / ((1 - x^2)^2 + (x/Q)^2)
    const auto x = frequencyHz / cutoffHz;
    return 10.0 * std::log10 (x * x * x * x / ((1.0 - x * x) * (1.0 - x * x) + (x / q) * (x / q)));
}
