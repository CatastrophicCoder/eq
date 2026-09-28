#include "PluginEditor.h"

#include "Parameters.h"

// Not implemented yet.
ParametricEQAudioProcessorEditor::ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      eqProcessor (p),
      output (p.getValueTreeState(), [&p] { return p.getAutoGainOffsetDb(); })
{
    for (int band = 1; band <= Parameters::numBands; ++band)
        strips[static_cast<size_t> (band - 1)] = std::make_unique<BandStrip> (p.getValueTreeState(), band);

    setSize (400, 300);
}

ParametricEQAudioProcessorEditor::~ParametricEQAudioProcessorEditor() = default;

void ParametricEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void ParametricEQAudioProcessorEditor::resized() {}
void ParametricEQAudioProcessorEditor::refreshControls() {}
