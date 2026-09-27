#include "TiltShelfDesign.h"

#include "MatchedOnePoleShelfDesign.h"

#include <cmath>

BiquadCoefficients TiltShelfDesign::design (double cornerHz, double gainDb, double sampleRate) noexcept
{
    auto c = MatchedOnePoleShelfDesign::designHigh (cornerHz, gainDb, sampleRate);

    // Scale by 1/sqrt(G): DC moves to -gain/2, the high plateau to +gain/2.
    const auto scale = std::pow (10.0, -gainDb / 40.0);
    c.b0 *= scale;
    c.b1 *= scale;
    return c;
}

double TiltShelfDesign::analogMagnitudeDb (double frequencyHz, double cornerHz, double gainDb) noexcept
{
    return MatchedOnePoleShelfDesign::analogHighMagnitudeDb (frequencyHz, cornerHz, gainDb) - gainDb / 2.0;
}
