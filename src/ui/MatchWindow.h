#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

//==============================================================================
/** Floating window for the EQ Match panel (M9e, decision 2026-09-29). Shows a
    component it does not own; closing it calls onClose. Message thread only.
*/
class MatchWindow final : public juce::DocumentWindow
{
public:
    MatchWindow (juce::Component& content, juce::Component* near);

    std::function<void()> onClose;
    void closeButtonPressed() override;
};
