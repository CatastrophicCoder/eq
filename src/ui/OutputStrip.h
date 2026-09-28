#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

//==============================================================================
/** Output gain, Auto Gain (with the current correction shown) and phase invert. */
class OutputStrip final : public juce::Component
{
public:
    /** offsetDb returns the latest Auto Gain offset (read on the message thread). */
    OutputStrip (juce::AudioProcessorValueTreeState& state, std::function<float()> offsetDb);
    ~OutputStrip() override;

    /** Updates the Auto Gain readout. Call on the message thread. */
    void refresh();

    void resized() override;

    juce::Slider& getGainSlider() noexcept         { return gain; }
    juce::ToggleButton& getAutoGainButton() noexcept { return autoGain; }
    juce::ToggleButton& getInvertButton() noexcept { return invert; }
    juce::Label& getOffsetLabel() noexcept         { return offsetLabel; }

private:
    juce::AudioProcessorValueTreeState& state;
    std::function<float()> offsetDb;

    juce::Slider gain;
    juce::Label gainCaption, offsetLabel;
    juce::ToggleButton autoGain, invert;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment, invertAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputStrip)
};
