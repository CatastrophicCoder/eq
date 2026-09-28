#include "LinearPhaseDesigner.h"

void LinearPhaseDesigner::design (std::span<const BandSettings>, double, int numTaps, Result& result)
{
    result.numTaps = numTaps;
}
