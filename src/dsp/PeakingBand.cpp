#include "PeakingBand.h"

PeakingBand::PeakingBand() = default;

// Not implemented yet.
void PeakingBand::prepare (double newSampleRate, int numChannels)
{
    juce::ignoreUnused (newSampleRate, numChannels);
}

void PeakingBand::setTargets (double frequencyHz, double newGainDb, double newQ) noexcept
{
    juce::ignoreUnused (frequencyHz, newGainDb, newQ);
}

void PeakingBand::reset() noexcept
{
}

void PeakingBand::process (juce::AudioBuffer<float>& buffer) noexcept
{
    juce::ignoreUnused (buffer);
}

void PeakingBand::updateCoefficients (double frequencyHz, double newGainDb, double newQ) noexcept
{
    juce::ignoreUnused (frequencyHz, newGainDb, newQ);
}
