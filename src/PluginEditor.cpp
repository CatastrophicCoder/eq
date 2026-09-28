#include "PluginEditor.h"

ParametricEQAudioProcessorEditor::ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      display (p),
      bandPanel (p.getValueTreeState()),
      bottomBar (p.getValueTreeState(), [&p] { return p.getAutoGainOffsetDb(); })
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (topBar);
    addAndMakeVisible (display);
    addAndMakeVisible (bottomBar);
    addAndMakeVisible (bandPanel);   // after the display: drawn on top of it

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
    setSize (defaultWidth, defaultHeight);

    refreshControls();
    startTimerHz (30);
}

ParametricEQAudioProcessorEditor::~ParametricEQAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ParametricEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour { 0xff17171d });
}

void ParametricEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    const auto h = getHeight();
    const auto w = getWidth();

    topBar.setBounds (area.removeFromTop (juce::jlimit (26, 44, h * 45 / 1000)));
    bottomBar.setBounds (area.removeFromBottom (juce::jlimit (24, 36, h * 35 / 1000)));
    display.setBounds (area);

    // Band panel: centred over the lower part of the display, above the frequency labels.
    const auto panelWidth = juce::jlimit (520, 820, w * 40 / 100);
    const auto panelHeight = juce::jlimit (118, 190, h * 19 / 100);
    const auto panelBottom = display.getBottom() - ResponseDisplay::labelStripHeight - 8;
    bandPanel.setBounds (display.getX() + (display.getWidth() - panelWidth) / 2, panelBottom - panelHeight,
                         panelWidth, panelHeight);
}

void ParametricEQAudioProcessorEditor::refreshControls()
{
    display.refresh();
    bandPanel.refreshControlStates();
    bottomBar.refresh();
}
