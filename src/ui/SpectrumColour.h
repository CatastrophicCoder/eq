#pragma once

#include <juce_graphics/juce_graphics.h>

//==============================================================================
/** Colours of visible light, for the band colours (decision 2026-09-28): band 1 is
    dark violet, band 16 red, the bands between at evenly spaced wavelengths.

    Wavelength to RGB: Dan Bruton, "Approximate RGB values for Visible Wavelengths"
    (1996): piecewise-linear primaries over 380-780 nm, intensity falling off towards
    both ends of vision, gamma 0.8.
*/
namespace SpectrumColour
{
    inline constexpr double firstWavelengthNm = 390.0;   // band 1: dark violet
    inline constexpr double lastWavelengthNm = 700.0;    // band 16: red

    juce::Colour fromWavelength (double nanometres);

    /** Evenly spaced wavelengths from firstWavelengthNm (band 1) to lastWavelengthNm (band numBands). */
    double wavelengthForBand (int bandNumber, int numBands) noexcept;
}
