#include "CascadeProcessor.h"

// Not implemented yet.
void CascadeProcessor::setCoefficients (const SectionCascade& newCascade) noexcept
{
    (void) newCascade;
}

void CascadeProcessor::reset() noexcept
{
}

double CascadeProcessor::processSample (int channel, double input) noexcept
{
    (void) channel;
    return input;
}
