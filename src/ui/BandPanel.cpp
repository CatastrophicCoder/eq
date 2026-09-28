#include "BandPanel.h"

#include "MenuLabels.h"
#include "Parameters.h"
#include "ResponseDisplay.h"

#include <cmath>

namespace
{
    void setUpKnob (juce::Slider& knob, juce::Label& caption, const juce::String& text)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        caption.setText (text, juce::dontSendNotification);
        caption.setJustificationType (juce::Justification::centred);
        caption.setFont (juce::FontOptions (12.0f));
    }
}

BandPanel::BandPanel (juce::AudioProcessorValueTreeState& s)
    : state (s)
{
    setName ("bandPanel");

    enable.setButtonText ("On");
    enable.setName ("enable");

    for (int i = 0; i < FilterTypes::count; ++i)
        type.addItem (FilterTypes::names[i], i + 1);
    for (int i = 0; i < CutSlope::count; ++i)
        slope.addItem (CutSlope::labels[i], i + 1);
    type.setName ("type");
    slope.setName ("slope");

    setUpKnob (frequency, frequencyCaption, "Freq");
    setUpKnob (gain, gainCaption, "Gain");
    setUpKnob (q, qCaption, "Q");
    frequency.setName ("freq");
    gain.setName ("gain");
    q.setName ("q");

    for (auto* c : std::initializer_list<juce::Component*> { &enable, &type, &frequencyCaption, &frequency, &gainCaption,
                                                              &gain, &qCaption, &q, &slope })
        addAndMakeVisible (c);

    setBand (1);
}

BandPanel::~BandPanel() = default;

void BandPanel::setBand (int bandNumber)
{
    band = juce::jlimit (1, 16, bandNumber);

    attach();
    refreshControlStates();
    repaint();
}

void BandPanel::attach()
{
    // Drop the old attachments first, so loading the new band's values cannot write into the old band.
    enableAttachment.reset();
    typeAttachment.reset();
    slopeAttachment.reset();
    frequencyAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();

    enableAttachment    = std::make_unique<ButtonAttachment>   (state, Parameters::id (band, "enabled"), enable);
    typeAttachment      = std::make_unique<ComboBoxAttachment> (state, Parameters::id (band, "type"), type);
    slopeAttachment     = std::make_unique<ComboBoxAttachment> (state, Parameters::id (band, "slope"), slope);
    frequencyAttachment = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "freq"), frequency);
    gainAttachment      = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "gain"), gain);
    qAttachment         = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "q"), q);

    MenuLabels::chooseForType (type);
    MenuLabels::chooseForSlope (slope);
}

void BandPanel::refreshControlStates()
{
    const auto raw = state.getRawParameterValue (Parameters::id (band, "type"))->load();
    const auto t = static_cast<FilterType> (juce::jlimit (0, FilterTypes::count - 1, juce::roundToInt (raw)));

    // A disabled band cannot be edited until it is enabled again: only its On switch stays active.
    const auto on = state.getRawParameterValue (Parameters::id (band, "enabled"))->load() >= 0.5f;

    type.setEnabled (on);
    frequency.setEnabled (on);
    frequencyCaption.setEnabled (on);
    gain.setEnabled (on && FilterTypes::usesGain (t));
    gainCaption.setEnabled (on && FilterTypes::usesGain (t));
    q.setEnabled (on && FilterTypes::usesQ (t));
    qCaption.setEnabled (on && FilterTypes::usesQ (t));
    slope.setEnabled (on && FilterTypes::usesSlope (t));
}

void BandPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour { 0xe0202029 });
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (ResponseDisplay::bandColour (band).withAlpha (0.6f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
}

void BandPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 6);


    // Menus on the sides, three knobs in the middle.
    const auto sideWidth = juce::jlimit (70, 130, area.getWidth() / 5);
    auto left = area.removeFromLeft (sideWidth);
    auto right = area.removeFromRight (sideWidth);

    enable.setBounds (left.removeFromTop (22));
    left.removeFromTop (6);
    type.setBounds (left.removeFromTop (22));

    right.removeFromTop (28);
    slope.setBounds (right.removeFromTop (22));

    const auto knobWidth = area.getWidth() / 3;
    for (auto [knob, caption] : { std::pair { &frequency, &frequencyCaption }, { &gain, &gainCaption }, { &q, &qCaption } })
    {
        auto column = area.removeFromLeft (knobWidth);
        caption->setBounds (column.removeFromTop (14));
        knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (64, column.getWidth()), 16);
        knob->setBounds (column);
    }

    MenuLabels::chooseForType (type);
    MenuLabels::chooseForSlope (slope);
}
