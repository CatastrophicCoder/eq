#include "MatchedOnePoleShelfDesign.h"

#include <cmath>
#include <numbers>

BiquadCoefficients MatchedOnePoleShelfDesign::designHigh (double cornerHz, double gainDb, double sampleRate) noexcept
{
    constexpr double pi = std::numbers::pi;
    constexpr double fm = 0.9;                          // matching point, eq. 12

    const auto G = std::pow (10.0, gainDb / 20.0);
    const auto fc = cornerHz / (sampleRate / 2.0);      // units of Nyquist
    const auto phiM = 1.0 - std::cos (pi * fm);

    // Eq. 12.
    const auto alpha = 2.0 / (pi * pi) * (1.0 / (fm * fm) + 1.0 / (G * fc * fc)) - 1.0 / phiM;
    const auto beta  = 2.0 / (pi * pi) * (1.0 / (fm * fm) + G / (fc * fc)) - 1.0 / phiM;

    // Eqs. 10-11.
    const auto a1 = -alpha / (1.0 + alpha + std::sqrt (1.0 + 2.0 * alpha));
    const auto b  = -beta  / (1.0 + beta  + std::sqrt (1.0 + 2.0 * beta));

    BiquadCoefficients c;
    c.a1 = a1;
    c.a2 = 0.0;
    c.b0 = (1.0 + a1) / (1.0 + b);
    c.b1 = b * c.b0;
    c.b2 = 0.0;
    return c;
}

double MatchedOnePoleShelfDesign::analogHighMagnitudeDb (double frequencyHz, double cornerHz, double gainDb) noexcept
{
    // Eq. 1: H(s) = (1 + sqrt(G) s) / (1 + s / sqrt(G)), s = j f/fc  ->  |H|^2 = (1 + G x^2) / (1 + x^2 / G)
    const auto G = std::pow (10.0, gainDb / 20.0);
    const auto x2 = (frequencyHz / cornerHz) * (frequencyHz / cornerHz);
    return 10.0 * std::log10 ((1.0 + G * x2) / (1.0 + x2 / G));
}
