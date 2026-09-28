#include "SpectrumColour.h"

#include <cmath>

juce::Colour SpectrumColour::fromWavelength (double nm)
{
    // Bruton (1996): piecewise-linear primaries ...
    double r = 0.0, g = 0.0, b = 0.0;

    if      (nm >= 380.0 && nm < 440.0) { r = (440.0 - nm) / 60.0; b = 1.0; }
    else if (nm >= 440.0 && nm < 490.0) { g = (nm - 440.0) / 50.0; b = 1.0; }
    else if (nm >= 490.0 && nm < 510.0) { g = 1.0; b = (510.0 - nm) / 20.0; }
    else if (nm >= 510.0 && nm < 580.0) { r = (nm - 510.0) / 70.0; g = 1.0; }
    else if (nm >= 580.0 && nm < 645.0) { r = 1.0; g = (645.0 - nm) / 65.0; }
    else if (nm >= 645.0 && nm <= 780.0) { r = 1.0; }

    // ... with intensity falling off near the limits of vision ...
    double intensity = 0.0;
    if      (nm >= 380.0 && nm < 420.0)  intensity = 0.3 + 0.7 * (nm - 380.0) / 40.0;
    else if (nm >= 420.0 && nm <= 700.0) intensity = 1.0;
    else if (nm > 700.0 && nm <= 780.0)  intensity = 0.3 + 0.7 * (780.0 - nm) / 80.0;

    // ... and gamma 0.8.
    auto channel = [intensity] (double c)
    {
        return static_cast<juce::uint8> (c > 0.0 ? juce::roundToInt (255.0 * std::pow (c * intensity, 0.8)) : 0);
    };

    return juce::Colour (channel (r), channel (g), channel (b));
}

double SpectrumColour::wavelengthForBand (int bandNumber, int numBands) noexcept
{
    if (numBands <= 1)
        return firstWavelengthNm;

    return firstWavelengthNm + (lastWavelengthNm - firstWavelengthNm) * (bandNumber - 1) / (numBands - 1.0);
}
