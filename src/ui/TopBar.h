#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** Thin bar above the display. M3: the plugin name only (presets, undo and A/B later). */
class TopBar final : public juce::Component
{
public:
    TopBar();
    void paint (juce::Graphics&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
};
