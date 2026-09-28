#include "AutoGain.h"

// Not implemented yet.
bool AutoGain::countsTowardsAutoGain (FilterType type) noexcept
{
    (void) type;
    return true;
}

double AutoGain::computeOffsetDb (std::span<const BandSettings> bands, double sampleRate) noexcept
{
    (void) bands; (void) sampleRate;
    return 0.0;
}
