#include "PluginEditor.h"

#include "Parameters.h"

namespace
{
    constexpr int titleHeight = 24;
    constexpr float outputColumnWeight = 1.4f;   // output strip is a bit wider than a band
}

ParametricEQAudioProcessorEditor::ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      output (p.getValueTreeState(), [&p] { return p.getAutoGainOffsetDb(); })
{
    setLookAndFeel (&lookAndFeel);

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        auto& strip = strips[static_cast<size_t> (band - 1)];
        strip = std::make_unique<BandStrip> (p.getValueTreeState(), band);
        addAndMakeVisible (*strip);
    }

    addAndMakeVisible (output);

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
    setSize (defaultWidth, defaultHeight);

    refreshControls();
    startTimerHz (15);
}

ParametricEQAudioProcessorEditor::~ParametricEQAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ParametricEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText (JucePlugin_Name, getLocalBounds().removeFromTop (titleHeight), juce::Justification::centred, 1);
}

void ParametricEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (6);
    area.removeFromTop (titleHeight);
    area.removeFromBottom (12);   // room for the corner resizer

    const auto unit = static_cast<float> (area.getWidth()) / (Parameters::numBands + outputColumnWeight);

    for (int i = 0; i < Parameters::numBands; ++i)
    {
        const auto x0 = area.getX() + juce::roundToInt (unit * static_cast<float> (i));
        const auto x1 = area.getX() + juce::roundToInt (unit * static_cast<float> (i + 1));
        strips[static_cast<size_t> (i)]->setBounds (x0, area.getY(), x1 - x0, area.getHeight());
    }

    const auto outputX = area.getX() + juce::roundToInt (unit * Parameters::numBands);
    output.setBounds (outputX, area.getY(), area.getRight() - outputX, area.getHeight());
}

void ParametricEQAudioProcessorEditor::refreshControls()
{
    for (auto& strip : strips)
        strip->refreshControlStates();

    output.refresh();
}
