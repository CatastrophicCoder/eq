#include "MatchedPeakingDesign.h"

#include <cmath>

BiquadCoefficients MatchedPeakingDesign::design (double centreHz, double gainDb, double q, double sampleRate) noexcept
{
    // Not implemented yet.
    (void) centreHz; (void) gainDb; (void) q; (void) sampleRate;
    return {};
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
