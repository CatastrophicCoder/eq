#include "OutputStrip.h"

// Not implemented yet.
OutputStrip::OutputStrip (juce::AudioProcessorValueTreeState& s, std::function<float()> offset)
    : state (s), offsetDb (std::move (offset)) {}
OutputStrip::~OutputStrip() = default;
void OutputStrip::refresh() {}
void OutputStrip::resized() {}
