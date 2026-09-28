#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

//==============================================================================
/** Thin bar below the display: output gain, Auto Gain (with its current
    correction) and phase invert.
*/
class BottomBar final : public juce::Component
{
public:
    BottomBar (juce::AudioProcessorValueTreeState& state, std::function<float()> autoGainOffsetDb);
    ~BottomBar() override;

    /** Updates the Auto Gain readout. Message thread only. */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider& getGainSlider() noexcept           { return gain; }
    juce::ToggleButton& getAutoGainButton() noexcept { return autoGain; }
    juce::ToggleButton& getInvertButton() noexcept   { return invert; }
    juce::Label& getOffsetLabel() noexcept           { return offsetLabel; }

private:
    juce::AudioProcessorValueTreeState& state;
    std::function<float()> offsetDb;

    juce::Slider gain;
    juce::Label gainCaption, offsetLabel;
    juce::ToggleButton autoGain, invert;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment, invertAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BottomBar)
};
