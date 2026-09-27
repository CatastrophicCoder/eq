#pragma once

#include "FilterType.h"

//==============================================================================
/** Everything that defines one band. Frequency, gain and Q are continuous
    (smoothed); type, slope and enabled are discrete (crossfaded).
*/
struct BandSettings
{
    FilterType type = FilterType::bell;
    double frequencyHz = 1000.0;
    double gainDb = 0.0;
    double q = 0.71;
    int slopeIndex = 1;   // 12 dB/oct
    bool enabled = true;

    bool hasSameDiscreteSettings (const BandSettings& other) const noexcept
    {
        return type == other.type && slopeIndex == other.slopeIndex && enabled == other.enabled;
    }
};
