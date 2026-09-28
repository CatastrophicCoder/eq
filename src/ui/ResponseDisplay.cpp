#include "ResponseDisplay.h"

#include "PluginProcessor.h"

// Not implemented yet.
ResponseDisplay::ResponseDisplay (ParametricEQAudioProcessor& p) : processor (p) {}
ResponseDisplay::~ResponseDisplay() = default;
void ResponseDisplay::refresh() {}
juce::Rectangle<float> ResponseDisplay::getPlotArea() const { return getLocalBounds().toFloat(); }
FrequencyAxis ResponseDisplay::getAxis() const { return { getPlotArea(), FrequencyAxis::defaultRangeDb }; }
juce::Path ResponseDisplay::getSumPath() const { return {}; }
juce::Colour ResponseDisplay::bandColour (int) { return juce::Colours::white; }
juce::Colour ResponseDisplay::sumColour() { return juce::Colours::white; }
void ResponseDisplay::paint (juce::Graphics&) {}
void ResponseDisplay::resized() {}
void ResponseDisplay::cycleRange() {}
void ResponseDisplay::updateRangeButton() {}
