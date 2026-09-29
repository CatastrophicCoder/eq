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

    auto fill = [] (juce::ComboBox& box, const auto& names, const juce::String& name, const juce::String& suffix = {})
    {
        int id = 1;
        for (auto* text : names)
            box.addItem (juce::String (text) + suffix, id++);
        box.setName (name);
    };

    fill (analyzerMode, AnalyzerSettings::modeNames, "analyzerMode");
    fill (resolution, AnalyzerSettings::resolutionNames, "resolution");
    fill (speed, AnalyzerSettings::speedNames, "speed");
    int id = 1;
    for (auto r : AnalyzerSettings::ranges)
        range.addItem (juce::String (juce::roundToInt (r)) + " dB", id++);
    range.setName ("range");
    analyzerMode.setTooltip ("Analyzer: which signal is shown");
    resolution.setTooltip ("Analyzer resolution (FFT size in points)");
    speed.setTooltip ("Analyzer speed (how fast the display falls)");
    range.setTooltip ("Analyzer dB range");
    freeze.setTooltip ("Hold the current spectrum");
    freeze.setButtonText ("Freeze");
    freeze.setName ("freeze");
    matchButton.setName ("match");
    matchButton.setTooltip ("EQ Match: learn a reference and fit bands to it");

    for (auto* box : { &analyzerMode, &resolution, &speed, &range })
        box->onChange = [this] { if (onAnalyzerSettingsChanged != nullptr) onAnalyzerSettingsChanged(); };
    freeze.onClick = [this] { if (onAnalyzerSettingsChanged != nullptr) onAnalyzerSettingsChanged(); };

    for (auto* c : std::initializer_list<juce::Component*> { &autoGain, &offsetLabel, &invert, &analyzerMode, &resolution,
                                                              &speed, &range, &freeze, &matchButton, &gainCaption, &gain })
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

void BottomBar::showAnalyzerSettings (const AnalyzerSettings::Values& v)
{
    analyzerMode.setSelectedItemIndex (v.mode, juce::dontSendNotification);
    resolution.setSelectedItemIndex (v.resolution, juce::dontSendNotification);
    speed.setSelectedItemIndex (v.speed, juce::dontSendNotification);
    range.setSelectedItemIndex (v.range, juce::dontSendNotification);
}

AnalyzerSettings::Values BottomBar::getAnalyzerSettingsShown() const
{
    return { analyzerMode.getSelectedItemIndex(), resolution.getSelectedItemIndex(),
             speed.getSelectedItemIndex(), range.getSelectedItemIndex() };
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

    constexpr int gap = 6;
    auto place = [&] (juce::Component& c, int width) { c.setBounds (area.removeFromLeft (width)); area.removeFromLeft (gap); };

    place (autoGain, 92);
    place (offsetLabel, 56);
    place (invert, 70);
    area.removeFromLeft (10);

    // Analyzer controls in the middle, output gain on the right.
    auto right = area.removeFromRight (juce::jmin (240, area.getWidth() / 3));
    gainCaption.setBounds (right.removeFromLeft (52));
    gain.setBounds (right);

    const auto rowHeight = juce::jmin (22, area.getHeight());
    auto centred = [&] (juce::Component& c, int width)
    {
        c.setBounds (area.removeFromLeft (width).withSizeKeepingCentre (width, rowHeight));
        area.removeFromLeft (gap);
    };

    centred (analyzerMode, 84);
    centred (resolution, 76);
    centred (speed, 70);
    centred (range, 66);
    centred (freeze, 70);
    centred (matchButton, 60);
}
