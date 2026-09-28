#include "BottomBar.h"

#include "Parameters.h"

BottomBar::BottomBar (juce::AudioProcessorValueTreeState& s, std::function<float()> offset)
    : state (s), offsetDb (std::move (offset))
{
    setName ("bottomBar");

    gain.setSliderStyle (juce::Slider::LinearHorizontal);
    gain.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    gain.setName ("outputGain");
    gainCaption.setText ("Output", juce::dontSendNotification);
    gainCaption.setJustificationType (juce::Justification::centredRight);
    gainCaption.setName ("outputCaption");
    autoGain.setButtonText ("Auto Gain");
    autoGain.setName ("autoGain");
    invert.setButtonText ("Invert");
    invert.setName ("invert");
    offsetLabel.setJustificationType (juce::Justification::centredLeft);
    offsetLabel.setName ("offset");

    for (auto* c : std::initializer_list<juce::Component*> { &autoGain, &offsetLabel, &invert, &gainCaption, &gain })
        addAndMakeVisible (c);

    gainAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, Parameters::outputGain, gain);
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, Parameters::autoGain, autoGain);
    invertAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, Parameters::outputInvert, invert);

    refresh();
}

BottomBar::~BottomBar() = default;

void BottomBar::refresh()
{
    const auto on = state.getRawParameterValue (Parameters::autoGain)->load() >= 0.5f;
    offsetLabel.setText (on ? juce::String (offsetDb(), 1) + " dB" : juce::String ("off"), juce::dontSendNotification);
}

void BottomBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour { 0xff111116 });
    g.setColour (juce::Colour { 0x14ffffff });
    g.drawHorizontalLine (0, 0.0f, static_cast<float> (getWidth()));
}

void BottomBar::resized()
{
    auto area = getLocalBounds().reduced (10, 2);

    autoGain.setBounds (area.removeFromLeft (100));
    offsetLabel.setBounds (area.removeFromLeft (70));
    area.removeFromLeft (10);
    invert.setBounds (area.removeFromLeft (80));

    gain.setBounds (area.removeFromRight (juce::jmin (260, area.getWidth() / 2)));
    gainCaption.setBounds (area.removeFromRight (60));
}
