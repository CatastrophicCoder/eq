#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class MatchSession;

//==============================================================================
/** Contents of the EQ Match window (M9e): learn buttons (one with a side-chain,
    reference and current without), learned seconds, Amount, Smoothing, Apply
    (which asks whether to replace all bands or keep them). Message thread only.
*/
class MatchPanel final : public juce::Component
{
public:
    explicit MatchPanel (MatchSession& session);

    /** Updates the buttons and counters from the session. */
    void refresh();

    juce::TextButton& getLearnReferenceButton() noexcept { return learnReference; }
    juce::TextButton& getLearnCurrentButton() noexcept   { return learnCurrent; }
    juce::TextButton& getApplyButton() noexcept          { return applyButton; }
    juce::Slider& getAmountSlider() noexcept             { return amount; }
    juce::Slider& getSmoothingSlider() noexcept          { return smoothing; }

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void showApplyChoice();

    MatchSession& session;
    juce::TextButton learnReference, learnCurrent, applyButton { "Apply..." };
    juce::Label referenceStatus, currentStatus, amountCaption, smoothingCaption;
    juce::Slider amount, smoothing;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatchPanel)
};
