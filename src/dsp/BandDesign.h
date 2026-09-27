#pragma once

#include "BandSettings.h"
#include "SectionCascade.h"

//==============================================================================
/** Turns band settings into filter sections, dispatching to the design for the
    band's type. A disabled band has no sections (pass-through).
*/
class BandDesign
{
public:
    /** Highest centre/corner frequency used, as a fraction of the sample rate. */
    static constexpr double maxFrequencyRatio = 0.49;

    static SectionCascade design (const BandSettings& settings, double sampleRate) noexcept;
};
