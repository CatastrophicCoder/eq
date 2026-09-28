#pragma once

#include "BandSettings.h"

#include <span>

//==============================================================================
/** Static Auto Gain: the output offset that keeps the K-weighted loudness of
    pink noise unchanged by the EQ.

        P      = sum_k w_k |H(f_k)|^2 / sum_k w_k,   w_k = |K(f_k)|^2
        offset = clamp (-10 log10 P, -limitDb, +limitDb)

    f_k are numPoints log-spaced frequencies from 20 Hz to 20 kHz (equal weight
    per octave = pink noise), K is KWeighting, and H is the product of every
    enabled band except Low Cut and High Cut (decisions 2026-09-28).
*/
class AutoGain
{
public:
    static constexpr int numPoints = 256;
    static constexpr double limitDb = 24.0;

    static bool countsTowardsAutoGain (FilterType type) noexcept;

    static double computeOffsetDb (std::span<const BandSettings> bands, double sampleRate) noexcept;
};
