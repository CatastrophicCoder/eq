#include "BandStrip.h"

#include "Parameters.h"
#include "dsp/CutSlope.h"
#include "dsp/FilterType.h"

namespace
{
    // Short forms for narrow columns; index order as FilterTypes::names / CutSlope::labels.
    constexpr const char* shortTypeNames[FilterTypes::count] { "Bell", "LoShf", "HiShf", "LoCut", "HiCut",
                                                               "Notch", "BPass", "Tilt", "FlatT", "AllP" };
    constexpr const char* shortSlopeLabels[CutSlope::count] { "6", "12", "18", "24", "30", "36", "42", "48", "54",
                                                              "60", "66", "72", "78", "84", "90", "96", "BW" };

    /** Width available for text inside a combo box, as its own label lays it out. */
    int textWidth (juce::ComboBox& box)
    {
        if (auto* label = dynamic_cast<juce::Label*> (box.getChildComponent (0)))
            return label->getBorderSize().subtractedFrom (label->getLocalBounds()).getWidth();
        return box.getWidth();
    }

    template <size_t N>
    void chooseLabels (juce::ComboBox& box, const char* const (&full)[N], const char* const (&brief)[N])
    {
        auto* label = dynamic_cast<juce::Label*> (box.getChildComponent (0));
        const auto font = label != nullptr ? label->getFont() : juce::Font (juce::FontOptions (12.0f));
        const auto available = static_cast<float> (textWidth (box));

        bool allFit = true;
        for (auto* text : full)
            allFit = allFit && juce::GlyphArrangement::getStringWidth (font, text) <= available;

        // Read the selection first: getSelectedId() returns 0 once the shown text no longer
        // matches the item text, which changeItemText below causes.
        const auto selected = box.getSelectedId();

        for (size_t i = 0; i < N; ++i)
            box.changeItemText (static_cast<int> (i) + 1, allFit ? full[i] : brief[i]);

        // changeItemText does not refresh the shown text; re-show the selection.
        box.setSelectedId (selected, juce::dontSendNotification);
    }

    void setUpKnob (juce::Slider& knob, juce::Label& caption, const juce::String& text)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
        caption.setText (text, juce::dontSendNotification);
        caption.setJustificationType (juce::Justification::centred);
        caption.setFont (juce::FontOptions (12.0f));
    }
}

BandStrip::BandStrip (juce::AudioProcessorValueTreeState& s, int bandNumber)
    : state (s), band (bandNumber)
{
    setName ("band" + juce::String (band));

    enable.setButtonText (juce::String (band));
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

    // Attach after the menus have their items: the attachments map choice index <-> item.
    enableAttachment    = std::make_unique<ButtonAttachment>   (state, Parameters::id (band, "enabled"), enable);
    typeAttachment      = std::make_unique<ComboBoxAttachment> (state, Parameters::id (band, "type"), type);
    slopeAttachment     = std::make_unique<ComboBoxAttachment> (state, Parameters::id (band, "slope"), slope);
    frequencyAttachment = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "freq"), frequency);
    gainAttachment      = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "gain"), gain);
    qAttachment         = std::make_unique<SliderAttachment>   (state, Parameters::id (band, "q"), q);

    refreshControlStates();
}

BandStrip::~BandStrip() = default;

void BandStrip::refreshControlStates()
{
    const auto raw = state.getRawParameterValue (Parameters::id (band, "type"))->load();
    const auto t = static_cast<FilterType> (juce::jlimit (0, FilterTypes::count - 1, juce::roundToInt (raw)));

    gain.setEnabled (FilterTypes::usesGain (t));
    gainCaption.setEnabled (FilterTypes::usesGain (t));
    q.setEnabled (FilterTypes::usesQ (t));
    qCaption.setEnabled (FilterTypes::usesQ (t));
    slope.setEnabled (FilterTypes::usesSlope (t));
}

void BandStrip::resized()
{
    auto area = getLocalBounds().reduced (2);
    constexpr int rowHeight = 22, captionHeight = 14, gap = 3;

    enable.setBounds (area.removeFromTop (rowHeight));
    area.removeFromTop (gap);
    type.setBounds (area.removeFromTop (rowHeight));
    area.removeFromTop (gap);
    slope.setBounds (area.removeFromBottom (rowHeight));
    area.removeFromBottom (gap);

    const auto knobBlock = area.getHeight() / 3;
    const auto textBoxWidth = juce::jmax (40, area.getWidth());

    for (auto [knob, caption] : { std::pair { &frequency, &frequencyCaption }, { &gain, &gainCaption }, { &q, &qCaption } })
    {
        auto block = area.removeFromTop (knobBlock);
        caption->setBounds (block.removeFromTop (captionHeight));
        knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (textBoxWidth, block.getWidth()), 16);
        knob->setBounds (block);
    }

    updateMenuLabels();
}

void BandStrip::lookAndFeelChanged()
{
    updateMenuLabels();
}

void BandStrip::updateMenuLabels()
{
    chooseLabels (type, FilterTypes::names, shortTypeNames);
    chooseLabels (slope, CutSlope::labels, shortSlopeLabels);
}
