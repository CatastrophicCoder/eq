#pragma once

#include <juce_data_structures/juce_data_structures.h>

class ParametricEQAudioProcessor;

//==============================================================================
/** Snapshot of the plugin's setting as a ValueTree, and applying one (M9a/M9b).

    Scope::sound: all parameters, bands in use, phase mode and length.
    Scope::everything: also the view settings and the current preset's name.
    apply() sets parameters that differ as host edits and touches only what the
    snapshot contains. Message thread only.
*/
namespace SettingSnapshot
{
    enum class Scope { sound, everything };

    juce::ValueTree capture (const ParametricEQAudioProcessor& processor, Scope scope);
    void apply (ParametricEQAudioProcessor& processor, const juce::ValueTree& snapshot);
}
