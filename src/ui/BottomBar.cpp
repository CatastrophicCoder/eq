#include "BottomBar.h"

// Not implemented yet.
BottomBar::BottomBar (juce::AudioProcessorValueTreeState& s, std::function<float()> o) : state (s), offsetDb (std::move (o)) {}
BottomBar::~BottomBar() = default;
void BottomBar::refresh() {}
void BottomBar::paint (juce::Graphics&) {}
void BottomBar::resized() {}
