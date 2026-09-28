#include "BandPanel.h"

// Not implemented yet.
BandPanel::BandPanel (juce::AudioProcessorValueTreeState& s) : state (s) {}
BandPanel::~BandPanel() = default;
void BandPanel::setBand (int bandNumber) { band = bandNumber; }
void BandPanel::refreshControlStates() {}
void BandPanel::paint (juce::Graphics&) {}
void BandPanel::resized() {}
void BandPanel::attach() {}
