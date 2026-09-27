#pragma once

#include "SectionCascade.h"

//==============================================================================
/** Flat tilt: an approximately straight dB-per-octave line through the pivot.

    Own construction: 16 matched one-pole high shelves (MatchedOnePoleShelfDesign)
    at octave spacing from 2.5 Hz to about 82 kHz, each with 1 octave's worth of slope,
    offset so the pivot sits at 0 dB.

    The gain parameter sets the total tilt over the ten octaves 20 Hz - 20 kHz,
    so the slope is gainDb / 10 dB per octave.
*/
class FlatTiltDesign
{
public:
    static constexpr int numShelves = 16;
    static constexpr double lowestShelfHz = 2.5;

    static SectionCascade design (double pivotHz, double gainDb, double sampleRate) noexcept;

    /** The straight line the design approximates. */
    static double idealMagnitudeDb (double frequencyHz, double pivotHz, double gainDb) noexcept;
};
