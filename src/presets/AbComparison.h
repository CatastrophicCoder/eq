#pragma once

#include <juce_data_structures/juce_data_structures.h>

class ParametricEQAudioProcessor;

//==============================================================================
/** A/B comparison (M9a, decisions 2026-09-29): two slots, each a complete
    setting (all parameters, bands in use, phase mode and length, view settings,
    current preset). Switching stores the current setting in the active slot and
    applies the other; parameters that differ are set as host edits. Both slots
    are saved with the session. Message thread only.
*/
class AbComparison
{
public:
    enum class Slot { a, b };
    static constexpr const char* stateTag = "ABComparison";

    explicit AbComparison (ParametricEQAudioProcessor& processor);

    Slot getActive() const noexcept { return active; }
    void switchTo (Slot slot);
    void copyActiveToOther();

    /** The inactive slot and the active one, for the saved state. */
    juce::ValueTree toState() const;

    /** Restores from a saved state; an invalid one (older sessions) makes both slots the current setting. */
    void fromState (const juce::ValueTree& state);

    /** Snapshot of the current setting, and applying one. */
    juce::ValueTree capture() const;
    void apply (const juce::ValueTree& snapshot);

private:
    ParametricEQAudioProcessor& processor;
    Slot active = Slot::a;
    juce::ValueTree other;   // the inactive slot; invalid until first needed
};
