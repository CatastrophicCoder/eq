#include "Parameters.h"

#include <cmath>

namespace
{
    /** A range mapped logarithmically onto 0..1, so equal knob travel is an equal ratio. */
    juce::NormalisableRange<float> logRange (float start, float end)
    {
        return { start, end,
                 [] (float min, float max, float normalised) { return min * std::pow (max / min, normalised); },
                 [] (float min, float max, float value)      { return std::log (value / min) / std::log (max / min); },
                 nullptr };
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout Parameters::createLayout()
{
    using Attributes = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { band1Freq, versionHint }, "Band 1 Frequency",
        logRange (20.0f, 20000.0f), 1000.0f,
        Attributes().withLabel ("Hz")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, v < 1000.0f ? 1 : 0); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { band1Gain, versionHint }, "Band 1 Gain",
        juce::NormalisableRange<float> (-30.0f, 30.0f, 0.01f), 0.0f,
        Attributes().withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { band1Q, versionHint }, "Band 1 Q",
        logRange (0.1f, 18.0f), 0.71f,
        Attributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); })));

    return layout;
}
