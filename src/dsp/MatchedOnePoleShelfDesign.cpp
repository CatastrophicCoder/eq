#include "MatchedOnePoleShelfDesign.h"

#include <cmath>

BiquadCoefficients MatchedOnePoleShelfDesign::designHigh (double cornerHz, double gainDb, double sampleRate) noexcept
{
    // Not implemented yet.
    (void) cornerHz; (void) gainDb; (void) sampleRate;
    return {};
}

double MatchedOnePoleShelfDesign::analogHighMagnitudeDb (double frequencyHz, double cornerHz, double gainDb) noexcept
{
    // Eq. 1: H(s) = (1 + sqrt(G) s) / (1 + s / sqrt(G)), s = j f/fc  ->  |H|^2 = (1 + G x^2) / (1 + x^2 / G)
    const auto G = std::pow (10.0, gainDb / 20.0);
    const auto x2 = (frequencyHz / cornerHz) * (frequencyHz / cornerHz);
    return 10.0 * std::log10 ((1.0 + G * x2) / (1.0 + x2 / G));
}
