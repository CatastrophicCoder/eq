#include "MatchedBandpassDesign.h"

#include <cmath>

BiquadCoefficients MatchedBandpassDesign::design (double centreHz, double q, double sampleRate) noexcept
{
    // Not implemented yet.
    (void) centreHz; (void) q; (void) sampleRate;
    return {};
}

double MatchedBandpassDesign::analogMagnitudeDb (double frequencyHz, double centreHz, double q) noexcept
{
    // H(s) = s w0/Q / (w0^2 + s w0/Q + s^2)  ->  |H|^2 = (x/Q)^2 / ((1 - x^2)^2 + (x/Q)^2)
    const auto x = frequencyHz / centreHz;
    const auto num = (x / q) * (x / q);
    return 10.0 * std::log10 (num / ((1.0 - x * x) * (1.0 - x * x) + num));
}
