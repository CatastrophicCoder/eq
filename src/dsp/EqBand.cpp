#include "EqBand.h"

// Not implemented yet.
EqBand::EqBand() = default;

void EqBand::prepare (double newSampleRate, int newNumChannels)
{
    juce::ignoreUnused (newSampleRate, newNumChannels);
}

void EqBand::setTargets (const BandSettings& newTargets) noexcept
{
    juce::ignoreUnused (newTargets);
}

void EqBand::reset() noexcept
{
}

void EqBand::process (juce::AudioBuffer<float>& buffer) noexcept
{
    juce::ignoreUnused (buffer);
}

const SectionCascade& EqBand::getActiveCascade() const noexcept
{
    return slots[static_cast<size_t> (active)].processor.getCascade();
}

BandSettings EqBand::getCurrentSettings() const noexcept
{
    return target;
}

void EqBand::startCrossfade() noexcept {}
void EqBand::updateCoefficients() noexcept {}
BandSettings EqBand::settingsFor (const Slot& slot) const noexcept { return slot.discrete; }
