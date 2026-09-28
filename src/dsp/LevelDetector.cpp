#include "LevelDetector.h"

#include <algorithm>
#include <cmath>

namespace
{
    double coefficientFor (double seconds, double sampleRate) noexcept
    {
        return seconds > 0.0 ? std::exp (-1.0 / (seconds * sampleRate)) : 0.0;
    }
}

void LevelDetector::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    rmsCoeff = coefficientFor (rmsSeconds, sampleRate);
    reset();
}

void LevelDetector::setTimes (double attackMs, double releaseMs) noexcept
{
    attackCoeff = coefficientFor (attackMs / 1000.0, sampleRate);
    releaseCoeff = coefficientFor (releaseMs / 1000.0, sampleRate);
}

void LevelDetector::reset() noexcept
{
    meanSquare = 0.0;
    envelopeDb = floorDb;
}

double LevelDetector::process (double input) noexcept
{
    double linear;

    if (mode == Mode::rms)
    {
        meanSquare = rmsCoeff * meanSquare + (1.0 - rmsCoeff) * input * input;
        linear = std::sqrt (meanSquare);
    }
    else
    {
        linear = std::abs (input);
    }

    const auto levelDb = linear > 0.0 ? std::max (floorDb, 20.0 * std::log10 (linear)) : floorDb;

    // Attack when rising, release when falling; both in dB.
    const auto coeff = levelDb > envelopeDb ? attackCoeff : releaseCoeff;
    envelopeDb = levelDb + coeff * (envelopeDb - levelDb);
    return envelopeDb;
}
