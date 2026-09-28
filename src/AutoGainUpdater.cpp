#include "AutoGainUpdater.h"

// Not implemented yet.
AutoGainUpdater::AutoGainUpdater (juce::AudioProcessorValueTreeState& state)
    : juce::Thread ("Auto Gain")
{
    juce::ignoreUnused (state);
}

AutoGainUpdater::~AutoGainUpdater() = default;

void AutoGainUpdater::setSampleRate (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
}

void AutoGainUpdater::run()
{
}

std::array<BandSettings, 16> AutoGainUpdater::snapshot() const
{
    return {};
}
