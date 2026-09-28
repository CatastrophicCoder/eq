#include "PluginEditor.h"

// Not implemented yet.
ParametricEQAudioProcessorEditor::ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      display (p),
      bandPanel (p.getValueTreeState()),
      bottomBar (p.getValueTreeState(), [&p] { return p.getAutoGainOffsetDb(); })
{
    setSize (400, 300);
}

ParametricEQAudioProcessorEditor::~ParametricEQAudioProcessorEditor() = default;

void ParametricEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void ParametricEQAudioProcessorEditor::resized() {}
void ParametricEQAudioProcessorEditor::refreshControls() {}
