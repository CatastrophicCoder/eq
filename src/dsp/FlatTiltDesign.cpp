#include "FlatTiltDesign.h"

#include <cmath>

SectionCascade FlatTiltDesign::design (double pivotHz, double gainDb, double sampleRate) noexcept
{
    // Not implemented yet.
    (void) pivotHz; (void) gainDb; (void) sampleRate;
    return {};
}

double FlatTiltDesign::idealMagnitudeDb (double frequencyHz, double pivotHz, double gainDb) noexcept
{
    return gainDb / 10.0 * std::log2 (frequencyHz / pivotHz);
}
