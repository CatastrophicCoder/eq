#include "PhaseModeControls.h"

#include "dsp/LinearPhaseEngine.h"

PhaseModeControls::PhaseModeControls()
{
    setName ("phaseMode");

    mode.addItemList ({ "Zero latency", "Linear phase" }, 1);
    mode.setName ("phaseModeBox");
    mode.setTooltip ("Phase mode: Linear phase keeps phase intact at the cost of latency");
    mode.onChange = [this]
    {
        length.setEnabled (mode.getSelectedItemIndex() == 1);
        if (onModeChanged != nullptr)
            onModeChanged (mode.getSelectedItemIndex() == 1);
    };

    for (size_t i = 0; i < LinearPhaseDesigner::tapCounts.size(); ++i)
        length.addItem ("-", static_cast<int> (i) + 1);
    length.setName ("phaseLengthBox");
    length.setTooltip ("Linear-phase latency: longer is more precise in the bass (8192 / 16384 / 32768 taps)");
    length.onChange = [this]
    {
        if (onLengthChanged != nullptr)
            onLengthChanged (length.getSelectedItemIndex());
    };

    addAndMakeVisible (mode);
    addAndMakeVisible (length);
    show (false, 0, 48000.0);
}

void PhaseModeControls::show (bool linear, int lengthIndex, double sampleRate)
{
    if (! juce::exactlyEqual (sampleRate, shownRate))
    {
        // Read the selection first: changing item text resets what getSelectedId() returns.
        const auto selected = length.getSelectedId();
        for (size_t i = 0; i < LinearPhaseDesigner::tapCounts.size(); ++i)
        {
            const auto ms = 1000.0 * LinearPhaseEngine::latencyFor (LinearPhaseDesigner::tapCounts[i]) / sampleRate;
            length.changeItemText (static_cast<int> (i) + 1, juce::String (juce::roundToInt (ms)) + " ms");
        }
        length.setSelectedId (selected, juce::dontSendNotification);
        shownRate = sampleRate;
    }

    mode.setSelectedItemIndex (linear ? 1 : 0, juce::dontSendNotification);
    length.setSelectedItemIndex (lengthIndex, juce::dontSendNotification);
    length.setEnabled (linear);
}

void PhaseModeControls::resized()
{
    auto area = getLocalBounds();
    length.setBounds (area.removeFromRight (juce::jmin (84, area.getWidth() / 3)));
    area.removeFromRight (6);
    mode.setBounds (area);
}
