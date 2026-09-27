#include "PluginEditor.h"

//==============================================================================
ParametricEQAudioProcessorEditor::ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor& p)
    : AudioProcessorEditor (&p)
{
    setSize (400, 300);
}

//==============================================================================
void ParametricEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText (JucePlugin_Name, getLocalBounds(), juce::Justification::centred, 1);
}

void ParametricEQAudioProcessorEditor::resized()
{
}
