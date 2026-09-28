#include "SpectrumColour.h"

// Not implemented yet.
juce::Colour SpectrumColour::fromWavelength (double) { return juce::Colours::white; }
double SpectrumColour::wavelengthForBand (int, int) noexcept { return 0.0; }
