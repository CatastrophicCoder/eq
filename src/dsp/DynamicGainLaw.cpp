#include "DynamicGainLaw.h"

#include <algorithm>
#include <cmath>

double DynamicGainLaw::gainChangeDb (Mode mode, double levelDb, double thresholdDb, double rangeDb, double ratio) noexcept
{
    const auto over = levelDb - thresholdDb;

    if (mode == Mode::range)
    {
        // Smoothstep from the threshold to 12 dB above it.
        const auto t = std::clamp (over / rangeSpanDb, 0.0, 1.0);
        return rangeDb * t * t * (3.0 - 2.0 * t);
    }

    // Ratio: compressor curve with a soft knee centred on the threshold.
    const auto slope = ratio > 1.0 ? 1.0 - 1.0 / ratio : 0.0;
    const auto halfKnee = ratioKneeDb / 2.0;

    double amount;
    if (over <= -halfKnee)
        amount = 0.0;
    else if (over >= halfKnee)
        amount = over * slope;
    else
        amount = slope * (over + halfKnee) * (over + halfKnee) / (2.0 * ratioKneeDb);

    // Capped at |range|, in the direction of the range.
    const auto capped = std::min (amount, std::abs (rangeDb));
    return rangeDb < 0.0 ? -capped : (rangeDb > 0.0 ? capped : 0.0);
}
