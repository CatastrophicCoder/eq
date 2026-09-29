#include "AbComparison.h"

#include "PluginProcessor.h"

AbComparison::AbComparison (ParametricEQAudioProcessor& p) : processor (p) {}
void AbComparison::switchTo (Slot) {}
void AbComparison::copyActiveToOther() {}
juce::ValueTree AbComparison::toState() const { return {}; }
void AbComparison::fromState (const juce::ValueTree&) {}
juce::ValueTree AbComparison::capture() const { return {}; }
void AbComparison::apply (const juce::ValueTree&) {}
