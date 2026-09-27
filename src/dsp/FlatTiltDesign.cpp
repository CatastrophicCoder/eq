#include "FlatTiltDesign.h"

#include "MatchedOnePoleShelfDesign.h"

#include <cmath>

SectionCascade FlatTiltDesign::design (double pivotHz, double gainDb, double sampleRate) noexcept
{
    // One octave's worth of slope per shelf; shelves an octave apart from lowestShelfHz.
    const auto slopePerOctave = gainDb / 10.0;

    SectionCascade cascade;
    auto corner = lowestShelfHz;

    for (int i = 0; i < numShelves; ++i, corner *= 2.0)
        cascade.add (MatchedOnePoleShelfDesign::designHigh (corner, slopePerOctave, sampleRate));

    // Put the pivot at 0 dB by scaling the first section's numerator.
    const auto scale = std::pow (10.0, -cascade.magnitudeDb (pivotHz, sampleRate) / 20.0);
    cascade.sections[0].b0 *= scale;
    cascade.sections[0].b1 *= scale;
    return cascade;
}

double FlatTiltDesign::idealMagnitudeDb (double frequencyHz, double pivotHz, double gainDb) noexcept
{
    return gainDb / 10.0 * std::log2 (frequencyHz / pivotHz);
}
