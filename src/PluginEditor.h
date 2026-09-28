#pragma once

#include "PluginProcessor.h"
#include "ui/BandPanel.h"
#include "ui/BottomBar.h"
#include "ui/CompactLookAndFeel.h"
#include "ui/ResponseDisplay.h"
#include "ui/TopBar.h"

//==============================================================================
/** M3 editor. The overall layout follows the reference EQ (decision 2026-09-28):
    a thin top bar, a full-width response display with the dB scale on the right,
    a band panel over the lower part of the display, and a thin bottom bar.
    Components and styling are our own.
*/
class ParametricEQAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    static constexpr int defaultWidth = 1280, defaultHeight = 770;
    static constexpr int minWidth = 960, minHeight = 580;
    static constexpr int maxWidth = 2400, maxHeight = 1440;

    explicit ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor&);
    ~ParametricEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    /** Refreshes curves, greying and the Auto Gain readout (also run by a 30 Hz timer). */
    void refreshControls();

    TopBar& getTopBar() noexcept              { return topBar; }
    ResponseDisplay& getDisplay() noexcept    { return display; }
    BandPanel& getBandPanel() noexcept        { return bandPanel; }
    BottomBar& getBottomBar() noexcept        { return bottomBar; }

private:
    void timerCallback() override { refreshControls(); }

    CompactLookAndFeel lookAndFeel;   // declared first: outlives every child
    TopBar topBar;
    ResponseDisplay display;
    BandPanel bandPanel;
    BottomBar bottomBar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQAudioProcessorEditor)
};
