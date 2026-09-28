#include "BandStrip.h"

// Not implemented yet.
BandStrip::BandStrip (juce::AudioProcessorValueTreeState& s, int bandNumber) : state (s), band (bandNumber) {}
BandStrip::~BandStrip() = default;
void BandStrip::refreshControlStates() {}
void BandStrip::resized() {}
