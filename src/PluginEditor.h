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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQAudioProcessorEditor)
};
