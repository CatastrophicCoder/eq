#include "TiltShelfDesign.h"

#include "MatchedOnePoleShelfDesign.h"

BiquadCoefficients TiltShelfDesign::design (double cornerHz, double gainDb, double sampleRate) noexcept
{
    // Not implemented yet.
    (void) cornerHz; (void) gainDb; (void) sampleRate;
    return {};
}

double TiltShelfDesign::analogMagnitudeDb (double frequencyHz, double cornerHz, double gainDb) noexcept
{
    return MatchedOnePoleShelfDesign::analogHighMagnitudeDb (frequencyHz, cornerHz, gainDb) - gainDb / 2.0;
}
