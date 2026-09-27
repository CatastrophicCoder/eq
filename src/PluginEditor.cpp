#include "PluginEditor.h"

#include "Parameters.h"

//==============================================================================
ParametricEQAudioProcessorEditor::Knob::Knob (juce::AudioProcessorValueTreeState& state,
                                              const char* parameterId,
                                              const juce::String& caption)
    : slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      attachment (state, parameterId, slider)
{
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);

    label.setText (caption, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
}

//==============================================================================
ParametricEQAudioProcessorEditor::ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      frequency (p.getValueTreeState(), Parameters::band1Freq, "Frequency"),
      gain      (p.getValueTreeState(), Parameters::band1Gain, "Gain"),
      q         (p.getValueTreeState(), Parameters::band1Q,    "Q")
{
    for (auto* knob : { &frequency, &gain, &q })
    {
        addAndMakeVisible (knob->slider);
        addAndMakeVisible (knob->label);
    }

    setSize (420, 220);
}

//==============================================================================
void ParametricEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText (JucePlugin_Name, getLocalBounds().removeFromTop (30), juce::Justification::centred, 1);
}

void ParametricEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (25);

    const auto width = area.getWidth() / 3;

    for (auto* knob : { &frequency, &gain, &q })
    {
        auto column = area.removeFromLeft (width);
        knob->label.setBounds (column.removeFromTop (20));
        knob->slider.setBounds (column.reduced (5));
    }
}
