#include "SectionCascade.h"

void SectionCascade::add (const BiquadCoefficients& section) noexcept
{
    if (numSections < maxSections)
        sections[static_cast<size_t> (numSections++)] = section;
}

double SectionCascade::magnitudeDb (double frequencyHz, double sampleRate) const noexcept
{
    double sum = 0.0;

    for (int i = 0; i < numSections; ++i)
        sum += sections[static_cast<size_t> (i)].magnitudeDb (frequencyHz, sampleRate);

    return sum;
}

bool SectionCascade::isStable() const noexcept
{
    for (int i = 0; i < numSections; ++i)
        if (! sections[static_cast<size_t> (i)].isStable())
            return false;

    return true;
}
