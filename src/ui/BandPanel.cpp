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
    for (int i = 0; i < ChannelModes::count; ++i)
        channel.addItem (ChannelModes::names[i], i + 1);
    type.setName ("type");
    slope.setName ("slope");
    channel.setName ("channel");

    setUpKnob (frequency, frequencyCaption, "Freq");
    setUpKnob (gain, gainCaption, "Gain");
    setUpKnob (q, qCaption, "Q");
    frequency.setName ("freq");
    gain.setName ("gain");
    q.setName ("q");

    for (auto* c : std::initializer_list<juce::Component*> { &enable, &type, &channel, &frequencyCaption, &frequency,
                                                              &gainCaption, &gain, &qCaption, &q, &slope })
        addAndMakeVisible (c);

    // Dynamics (M7).
    dynamic.setButtonText ("Dynamic");
    dynamic.setName ("dyn");
    sidechain.setButtonText ("Side-chain");
    sidechain.setName ("sidechain");
    dynamicMode.addItemList ({ "Range", "Ratio" }, 1);
    detector.addItemList ({ "Peak", "RMS" }, 1);
    dynamicMode.setName ("dynmode");
    detector.setName ("detector");

    setUpKnob (threshold, thresholdCaption, "Thresh");
    setUpKnob (range, rangeCaption, "Range");
    setUpKnob (ratio, ratioCaption, "Ratio");
    setUpKnob (attack, attackCaption, "Attack");
    setUpKnob (release, releaseCaption, "Release");
    threshold.setName ("thresh");
    range.setName ("range");
    ratio.setName ("ratio");
    attack.setName ("attack");
    release.setName ("release");

    for (auto* c : std::initializer_list<juce::Component*> { &dynamic, &dynamicMode, &detector, &sidechain,
                                                              &thresholdCaption, &threshold, &rangeCaption, &range,
                                                              &ratioCaption, &ratio, &attackCaption, &attack,
                                                              &releaseCaption, &release })
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
    channelAttachment.reset();
    frequencyAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
    dynamicAttachment.reset();
    sidechainAttachment.reset();
    dynamicModeAttachment.reset();
    detectorAttachment.reset();
    thresholdAttachment.reset();
    rangeAttachment.reset();
    ratioAttachment.reset();
    attackAttachment.reset();
    releaseAttachment.reset();

    enableAttachment    = std::make_unique<ButtonAttachment>   (state, Parameters::id (band, "enabled"), enable);
    typeAttachment      = std::make_unique<ComboBoxAttachment> (state, Parameters::id (band, "type"), type);
    slopeAttachment     = std::make_unique<ComboBoxAttachment> (state, Parameters::id (band, "slope"), slope);
    channelAttachment   = std::make_unique<ComboBoxAttachment> (state, Parameters::id (band, "channel"), channel);
    frequencyAttachment = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "freq"), frequency);
    gainAttachment      = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "gain"), gain);
    qAttachment         = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "q"), q);

    dynamicAttachment     = std::make_unique<ButtonAttachment>   (state, Parameters::id (band, "dyn"), dynamic);
    sidechainAttachment   = std::make_unique<ButtonAttachment>   (state, Parameters::id (band, "sidechain"), sidechain);
    dynamicModeAttachment = std::make_unique<ComboBoxAttachment> (state, Parameters::id (band, "dynmode"), dynamicMode);
    detectorAttachment    = std::make_unique<ComboBoxAttachment> (state, Parameters::id (band, "detector"), detector);
    thresholdAttachment   = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "thresh"), threshold);
    rangeAttachment       = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "range"), range);
    ratioAttachment       = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "ratio"), ratio);
    attackAttachment      = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "attack"), attack);
    releaseAttachment     = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "release"), release);

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
    channel.setEnabled (on);
    frequency.setEnabled (on);
    frequencyCaption.setEnabled (on);
    gain.setEnabled (on && FilterTypes::usesGain (t));
    gainCaption.setEnabled (on && FilterTypes::usesGain (t));
    q.setEnabled (on && FilterTypes::usesQ (t));
    qCaption.setEnabled (on && FilterTypes::usesQ (t));
    slope.setEnabled (on && FilterTypes::usesSlope (t));

    // Dynamics: the switch for types that can be dynamic; its settings only while it is on.
    const auto canBeDynamic = on && BandSettings::typeCanBeDynamic (t);
    const auto dynOn = canBeDynamic && state.getRawParameterValue (Parameters::id (band, "dyn"))->load() >= 0.5f;
    const auto ratioMode = state.getRawParameterValue (Parameters::id (band, "dynmode"))->load() >= 0.5f;

    dynamic.setEnabled (canBeDynamic);
    for (auto* c : std::initializer_list<juce::Component*> { &dynamicMode, &detector, &sidechain, &threshold, &thresholdCaption,
                                                              &range, &rangeCaption, &attack, &attackCaption, &release, &releaseCaption })
        c->setEnabled (dynOn);
    ratio.setEnabled (dynOn && ratioMode);
    ratioCaption.setEnabled (dynOn && ratioMode);
}

void BandPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour { 0xe0202029 });
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (ResponseDisplay::bandColour (band).withAlpha (0.6f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    // Divider between the filter and the dynamics sections.
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawVerticalLine (dividerX, bounds.getY() + 8.0f, bounds.getBottom() - 8.0f);
}

void BandPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 6);

    // Filter section on the left (menus, three knobs, slope), dynamics on the right
    // (switches and menus, five knobs). Side columns share one width.
    const auto columnWidth = juce::jlimit (70, 130, area.getWidth() / 9);
    auto filter = area.removeFromLeft (area.getWidth() * 45 / 100);
    dividerX = filter.getRight() + 4;
    area.removeFromLeft (10);
    auto dyn = area;

    auto left = filter.removeFromLeft (columnWidth);
    auto right = filter.removeFromRight (columnWidth);

    auto stack = [] (juce::Rectangle<int>& column, juce::Component& c)
    {
        c.setBounds (column.removeFromTop (22));
        column.removeFromTop (5);
    };

    stack (left, enable);
    stack (left, type);
    stack (left, channel);

    right.removeFromTop (27);
    slope.setBounds (right.removeFromTop (22));

    auto layOutKnobs = [] (juce::Rectangle<int> row, std::initializer_list<std::pair<juce::Slider*, juce::Label*>> knobs)
    {
        const auto knobWidth = row.getWidth() / static_cast<int> (knobs.size());
        for (auto [knob, caption] : knobs)
        {
            auto column = row.removeFromLeft (knobWidth);
            caption->setBounds (column.removeFromTop (14));
            knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (64, column.getWidth() - 4), 16);
            knob->setBounds (column);
        }
    };

    layOutKnobs (filter, { { &frequency, &frequencyCaption }, { &gain, &gainCaption }, { &q, &qCaption } });

    auto switches = dyn.removeFromLeft (columnWidth);
    stack (switches, dynamic);
    stack (switches, dynamicMode);
    stack (switches, detector);
    stack (switches, sidechain);

    layOutKnobs (dyn, { { &threshold, &thresholdCaption }, { &range, &rangeCaption }, { &ratio, &ratioCaption },
                        { &attack, &attackCaption }, { &release, &releaseCaption } });

    MenuLabels::chooseForType (type);
    MenuLabels::chooseForSlope (slope);
}
