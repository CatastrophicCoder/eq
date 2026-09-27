#include "FirstOrderCutDesign.h"

#include "MatchedDesignMath.h"

#include <algorithm>
#include <cmath>

namespace
{
    /** Squared magnitude of 1 + a1 z^-1 on the unit circle at w. */
    double squaredDenominator (double a1, double w) noexcept
    {
        return 0.5 * ((1.0 + a1) * (1.0 + a1) * (1.0 + std::cos (w)) + (1.0 - a1) * (1.0 - a1) * (1.0 - std::cos (w)));
    }
}

BiquadCoefficients FirstOrderCutDesign::designLowpass (double cutoffHz, double sampleRate) noexcept
{
    const auto wc = MatchedDesignMath::omega (cutoffHz, sampleRate);
    const auto a1 = -std::exp (-wc);

    // |b0 + b1 e^-jw|^2 = ((b0 + b1)^2 (1 + cos w) + (b0 - b1)^2 (1 - cos w)) / 2.
    // Unity DC: b0 + b1 = 1 + a1 = r0. Half power at wc fixes r1 = b0 - b1.
    const auto r0 = 1.0 + a1;
    const auto r1Squared = (squaredDenominator (a1, wc) - r0 * r0 * (1.0 + std::cos (wc))) / (1.0 - std::cos (wc));
    const auto r1 = std::sqrt (std::max (r1Squared, 0.0));

    BiquadCoefficients c;
    c.a1 = a1;
    c.b0 = 0.5 * (r0 + r1);
    c.b1 = 0.5 * (r0 - r1);
    c.b2 = c.a2 = 0.0;
    return c;
}

BiquadCoefficients FirstOrderCutDesign::designHighpass (double cutoffHz, double sampleRate) noexcept
{
    const auto wc = MatchedDesignMath::omega (cutoffHz, sampleRate);
    const auto a1 = -std::exp (-wc);

    // Zero at DC (b1 = -b0): |H|^2 = 2 b0^2 (1 - cos w) / D(w). Half power at wc fixes b0.
    BiquadCoefficients c;
    c.a1 = a1;
    c.b0 = std::sqrt (0.5 * squaredDenominator (a1, wc) / (2.0 * (1.0 - std::cos (wc))));
    c.b1 = -c.b0;
    c.b2 = c.a2 = 0.0;
    return c;
}
