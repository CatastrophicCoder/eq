#pragma once

#include "PluginProcessor.h"
#include "ui/BandStrip.h"
#include "ui/CompactLookAndFeel.h"
#include "ui/OutputStrip.h"

#include <array>

//==============================================================================
/** M2 editor: one resizable row of 16 band strips plus the output strip, in the
    default JUCE look (the real UI comes in M3-M4).
*/
class ParametricEQAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    static constexpr int defaultWidth = 1480, defaultHeight = 440;
    static constexpr int minWidth = 1000, minHeight = 360;
    static constexpr int maxWidth = 2600, maxHeight = 800;

    explicit ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor&);
    ~ParametricEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    /** Greys out unused controls and updates the Auto Gain readout (also run by a 15 Hz timer). */
    void refreshControls();

    BandStrip& getBandStrip (int band) noexcept { return *strips[static_cast<size_t> (band - 1)]; }
    OutputStrip& getOutputStrip() noexcept      { return output; }

private:
    void timerCallback() override { refreshControls(); }

    ParametricEQAudioProcessor& eqProcessor;
    CompactLookAndFeel lookAndFeel;   // declared before the strips: outlives them
    std::array<std::unique_ptr<BandStrip>, 16> strips;
    OutputStrip output;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQAudioProcessorEditor)
};
