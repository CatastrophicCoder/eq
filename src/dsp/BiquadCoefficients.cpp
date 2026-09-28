#include "BiquadCoefficients.h"

#include <cmath>
#include <complex>
#include <numbers>

std::complex<double> BiquadCoefficients::response (double frequencyHz, double sampleRate) const noexcept
{
    return { 1.0, 0.0 };   // Not implemented yet.
}

double BiquadCoefficients::magnitudeDb (double frequencyHz, double sampleRate) const noexcept
{
    const auto w = 2.0 * std::numbers::pi * frequencyHz / sampleRate;
    const auto z1 = std::polar (1.0, -w);   // z^-1 on the unit circle
    const auto z2 = z1 * z1;

    const auto numerator   = b0 + b1 * z1 + b2 * z2;
    const auto denominator = 1.0 + a1 * z1 + a2 * z2;

    return 20.0 * std::log10 (std::abs (numerator) / std::abs (denominator));
}

bool BiquadCoefficients::isStable() const noexcept
{
    return std::abs (a1) < 1.0 + a2 && a2 < 1.0;
}
