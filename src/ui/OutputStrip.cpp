#include "OutputStrip.h"

#include "Parameters.h"

OutputStrip::OutputStrip (juce::AudioProcessorValueTreeState& s, std::function<float()> offset)
    : state (s), offsetDb (std::move (offset))
{
    setName ("output");

    gain.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gain.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    gain.setName ("outputGain");
    gainCaption.setText ("Output", juce::dontSendNotification);
    gainCaption.setJustificationType (juce::Justification::centred);
    autoGain.setButtonText ("Auto Gain");
    autoGain.setName ("autoGain");
    invert.setButtonText ("Invert");
    invert.setName ("invert");
    offsetLabel.setJustificationType (juce::Justification::centred);
    offsetLabel.setFont (juce::FontOptions (12.0f));
    offsetLabel.setName ("offset");

    for (auto* c : std::initializer_list<juce::Component*> { &gainCaption, &gain, &autoGain, &offsetLabel, &invert })
        addAndMakeVisible (c);

    gainAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, Parameters::outputGain, gain);
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, Parameters::autoGain, autoGain);
    invertAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, Parameters::outputInvert, invert);

    refresh();
}

OutputStrip::~OutputStrip() = default;

void OutputStrip::refresh()
{
    const auto on = state.getRawParameterValue (Parameters::autoGain)->load() >= 0.5f;
    offsetLabel.setText (on ? juce::String (offsetDb(), 1) + " dB" : juce::String ("off"), juce::dontSendNotification);
}

void OutputStrip::resized()
{
    auto area = getLocalBounds().reduced (2);
    constexpr int rowHeight = 22, gap = 3;

    invert.setBounds (area.removeFromBottom (rowHeight));
    area.removeFromBottom (gap);
    offsetLabel.setBounds (area.removeFromBottom (16));
    autoGain.setBounds (area.removeFromBottom (rowHeight));
    area.removeFromBottom (gap);
    gainCaption.setBounds (area.removeFromTop (14));
    gain.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (64, area.getWidth()), 16);
    gain.setBounds (area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), area.getWidth() + 30)));
}
