#include "MatchedShelfDesign.h"

#include <cmath>

BiquadCoefficients MatchedShelfDesign::designHigh (double cornerHz, double gainDb, double sampleRate) noexcept
{
    // Not implemented yet.
    (void) cornerHz; (void) gainDb; (void) sampleRate;
    return {};
}

BiquadCoefficients MatchedShelfDesign::designLow (double cornerHz, double gainDb, double sampleRate) noexcept
{
    // Not implemented yet.
    (void) cornerHz; (void) gainDb; (void) sampleRate;
    return {};
}

double MatchedShelfDesign::analogHighMagnitudeDb (double frequencyHz, double cornerHz, double gainDb) noexcept
{
    // Eq. 1 with g = G^(1/4), s = j f/fc  ->  |H|^2 = (1 + G x^4) / (1 + x^4 / G), x = f/fc
    const auto G = std::pow (10.0, gainDb / 20.0);
    const auto x4 = std::pow (frequencyHz / cornerHz, 4.0);
    return 10.0 * std::log10 ((1.0 + G * x4) / (1.0 + x4 / G));
}

double MatchedShelfDesign::analogLowMagnitudeDb (double frequencyHz, double cornerHz, double gainDb) noexcept
{
    // A low shelf with gain G is G times a high shelf with gain 1/G.
    return gainDb + analogHighMagnitudeDb (frequencyHz, cornerHz, -gainDb);
}
