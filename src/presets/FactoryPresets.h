#pragma once

#include "Preset.h"

#include <vector>

//==============================================================================
/** Built-in presets (decision 2026-09-28): starting points for common sources.
    Frequencies and cut/boost directions follow widely published mixing guidance
    (at least two independent public guides per preset); most dB and Q values are
    conservative choices. Starting points only: every source and mix differs.
*/
namespace FactoryPresets
{
    const std::vector<Preset>& all();

    /** Categories in display order. */
    const juce::StringArray& categories();
}
