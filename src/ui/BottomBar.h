#pragma once

#include "AnalyzerSettings.h"

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

    /** Shows stored analyzer settings in the menus (without notifying). */
    void showAnalyzerSettings (const AnalyzerSettings::Values& values);
    AnalyzerSettings::Values getAnalyzerSettingsShown() const;

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider& getGainSlider() noexcept           { return gain; }
    juce::ToggleButton& getAutoGainButton() noexcept { return autoGain; }
    juce::ToggleButton& getInvertButton() noexcept   { return invert; }
    juce::Label& getOffsetLabel() noexcept           { return offsetLabel; }

    juce::ComboBox& getAnalyzerModeBox() noexcept    { return analyzerMode; }
    juce::ComboBox& getResolutionBox() noexcept      { return resolution; }
    juce::ComboBox& getSpeedBox() noexcept           { return speed; }
    juce::ComboBox& getRangeBox() noexcept           { return range; }
    juce::ToggleButton& getFreezeButton() noexcept   { return freeze; }
    juce::TextButton& getMatchButton() noexcept      { return matchButton; }   // EQ Match window (M9e)

    /** Called when an analyzer control changes (the editor stores the settings). */
    std::function<void()> onAnalyzerSettingsChanged;

private:
    juce::AudioProcessorValueTreeState& state;
    std::function<float()> offsetDb;

    juce::Slider gain;
    juce::Label gainCaption, offsetLabel;
    juce::ToggleButton autoGain, invert;
    juce::ComboBox analyzerMode, resolution, speed, range;
    juce::ToggleButton freeze;
    juce::TextButton matchButton { "Match" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment, invertAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BottomBar)
};
