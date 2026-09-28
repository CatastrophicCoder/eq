#include "TopBar.h"

#include "ResponseDisplay.h"

TopBar::TopBar()
{
    setName ("topBar");
}

void TopBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour { 0xff111116 });
    g.setColour (juce::Colour { 0x14ffffff });
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));

    g.setColour (ResponseDisplay::sumColour());
    g.setFont (juce::FontOptions (static_cast<float> (getHeight()) * 0.5f, juce::Font::bold));
    g.drawText (JucePlugin_Name, getLocalBounds().reduced (14, 0), juce::Justification::centredLeft);
}
