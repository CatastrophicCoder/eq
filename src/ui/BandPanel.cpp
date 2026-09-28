#include "BandPanel.h"

#include "MenuLabels.h"
#include "Parameters.h"
#include "ResponseDisplay.h"

#include <cmath>

namespace
{
    constexpr int tabRadioGroup = 1601;

    void setUpKnob (juce::Slider& knob, juce::Label& caption, const juce::String& text)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        caption.setText (text, juce::dontSendNotification);
        caption.setJustificationType (juce::Justification::centred);
        caption.setFont (juce::FontOptions (12.0f));
    }
}

namespace
{
    /** WCAG 2 contrast ratio between two colours. */
    double contrastRatio (juce::Colour a, juce::Colour b)
    {
        auto luminance = [] (juce::Colour c)
        {
            auto lin = [] (juce::uint8 v)
            {
                const auto s = v / 255.0;
                return s <= 0.03928 ? s / 12.92 : std::pow ((s + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * lin (c.getRed()) + 0.7152 * lin (c.getGreen()) + 0.0722 * lin (c.getBlue());
        };

        const auto la = luminance (a), lb = luminance (b);
        return (std::max (la, lb) + 0.05) / (std::min (la, lb) + 0.05);
    }
}

juce::Colour BandPanel::tabBackground()
{
    return juce::Colour { 0xff2a2a33 };
}

juce::Colour BandPanel::tabTextColour (int bandNumber)
{
    // Same hue as the band; raise brightness, then lower saturation, until the number is readable
    // (WCAG contrast 4.5 for small text, with a little margin).
    auto colour = ResponseDisplay::bandColour (bandNumber);

    for (int step = 0; step < 40 && contrastRatio (colour, tabBackground()) < 4.8; ++step)
    {
        if (colour.getBrightness() < 1.0f)
            colour = colour.withBrightness (juce::jmin (1.0f, colour.getBrightness() + 0.05f));
        else
            colour = colour.withSaturation (juce::jmax (0.2f, colour.getSaturation() - 0.05f));
    }

    return colour;
}

BandPanel::BandPanel (juce::AudioProcessorValueTreeState& s)
    : state (s)
{
    setName ("bandPanel");

    for (int i = 1; i <= static_cast<int> (tabs.size()); ++i)
    {
        auto& tab = getTab (i);
        tab.setButtonText (juce::String (i));
        tab.setName ("tab" + juce::String (i));
        tab.setRadioGroupId (tabRadioGroup, juce::dontSendNotification);
        tab.setClickingTogglesState (false);
        tab.setColour (juce::TextButton::buttonColourId, tabBackground());
        tab.setColour (juce::TextButton::textColourOffId, tabTextColour (i));
        tab.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        tab.setColour (juce::TextButton::buttonOnColourId, ResponseDisplay::bandColour (i).withAlpha (0.45f));
        tab.onClick = [this, i] { setBand (i); };
        addAndMakeVisible (tab);
    }

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
    band = juce::jlimit (1, static_cast<int> (tabs.size()), bandNumber);

    for (int i = 1; i <= static_cast<int> (tabs.size()); ++i)
        getTab (i).setToggleState (i == band, juce::dontSendNotification);

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

    gain.setEnabled (FilterTypes::usesGain (t));
    gainCaption.setEnabled (FilterTypes::usesGain (t));
    q.setEnabled (FilterTypes::usesQ (t));
    qCaption.setEnabled (FilterTypes::usesQ (t));
    slope.setEnabled (FilterTypes::usesSlope (t));

    // Dim the tabs of bands that are switched off.
    for (int i = 1; i <= static_cast<int> (tabs.size()); ++i)
        getTab (i).setAlpha (state.getRawParameterValue (Parameters::id (i, "enabled"))->load() >= 0.5f ? 1.0f : 0.55f);
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

    // Tabs across the top.
    auto tabRow = area.removeFromTop (20);
    const auto tabWidth = static_cast<float> (tabRow.getWidth()) / static_cast<float> (tabs.size());
    for (int i = 0; i < static_cast<int> (tabs.size()); ++i)
    {
        const auto x0 = tabRow.getX() + juce::roundToInt (tabWidth * static_cast<float> (i));
        const auto x1 = tabRow.getX() + juce::roundToInt (tabWidth * static_cast<float> (i + 1));
        tabs[static_cast<size_t> (i)].setBounds (x0 + 1, tabRow.getY(), x1 - x0 - 2, tabRow.getHeight());
    }

    area.removeFromTop (6);

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
