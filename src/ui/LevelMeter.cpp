#include "LevelMeter.h"

#include <algorithm>
#include <cmath>

namespace
{
    double toDb (double linear, double floor) { return linear > 0.0 ? std::max (floor, 20.0 * std::log10 (linear)) : floor; }
}

void LevelMeter::addSamples (const float* left, const float* right, int numSamples, double sampleRate)
{
    const auto alpha = std::exp (-1.0 / (rmsSeconds * sampleRate));
    const float* channels[] { left, right };

    for (size_t ch = 0; ch < 2; ++ch)
    {
        auto ms = meanSquare[ch];
        auto blockPeak = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto x = static_cast<double> (channels[ch][i]);
            ms = alpha * ms + (1.0 - alpha) * x * x;
            blockPeak = std::max (blockPeak, std::abs (x));
        }

        meanSquare[ch] = ms;
        peak[ch] = std::max (peak[ch], toDb (blockPeak, floorDb));
        clipped[ch] = clipped[ch] || blockPeak > 1.0;
    }
}

void LevelMeter::update (double elapsedSeconds)
{
    for (size_t ch = 0; ch < 2; ++ch)
    {
        if (peak[ch] >= held[ch])
        {
            held[ch] = peak[ch];
            holdRemaining[ch] = holdSeconds;
        }
        else
        {
            // Hold, then fall. Time left over when the hold runs out already falls, and a
            // hold within rounding of zero counts as over (1.0 - 10 x 0.1 is not exactly 0).
            auto fallTime = elapsedSeconds;

            if (holdRemaining[ch] > 0.0)
            {
                holdRemaining[ch] -= elapsedSeconds;
                fallTime = holdRemaining[ch] <= 1.0e-9 ? -holdRemaining[ch] : 0.0;
                if (holdRemaining[ch] <= 1.0e-9)
                    holdRemaining[ch] = 0.0;
            }

            held[ch] = std::max (floorDb, held[ch] - peakFallDbPerSecond * std::max (0.0, fallTime));
        }

        peak[ch] = floorDb;   // the next update starts a new "highest since last update"
    }
}

double LevelMeter::peakDb (int channel) const noexcept
{
    return held[static_cast<size_t> (channel)];
}

double LevelMeter::rmsDb (int channel) const noexcept
{
    return toDb (std::sqrt (meanSquare[static_cast<size_t> (channel)]), floorDb);
}
