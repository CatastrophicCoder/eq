#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/** Parameter IDs and the parameter layout. IDs carry the band index. */
namespace Parameters
{
    inline constexpr const char* band1Freq = "band1_freq";
    inline constexpr const char* band1Gain = "band1_gain";
    inline constexpr const char* band1Q    = "band1_q";

    /** Version hint for juce::ParameterID; bump only when a parameter's meaning changes. */
    inline constexpr int versionHint = 1;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
