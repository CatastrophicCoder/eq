#pragma once

#include "PluginProcessor.h"

//==============================================================================
class ParametricEQAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor&);
    ~ParametricEQAudioProcessorEditor() override = default;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** A rotary slider with a caption, attached to one parameter. */
    struct Knob
    {
        Knob (juce::AudioProcessorValueTreeState& state, const char* parameterId, const juce::String& caption);

        juce::Slider slider;
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    Knob frequency, gain, q;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQAudioProcessorEditor)
};
