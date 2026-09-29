#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

//==============================================================================
/** Phase mode (Zero latency / Linear phase) and linear-phase length menus (M8).
    The length items show their latency in ms at the current sample rate. The
    editor connects them to the processor. Message thread only.
*/
class PhaseModeControls final : public juce::Component
{
public:
    PhaseModeControls();

    /** Shows the processor's state without notifying. */
    void show (bool linear, int lengthIndex, double sampleRate);

    /** Called when the user picks a mode or a length. */
    std::function<void (bool linear)> onModeChanged;
    std::function<void (int lengthIndex)> onLengthChanged;

    juce::ComboBox& getModeBox() noexcept   { return mode; }
    juce::ComboBox& getLengthBox() noexcept { return length; }

    void resized() override;

private:
    juce::ComboBox mode, length;
    double shownRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseModeControls)
};
