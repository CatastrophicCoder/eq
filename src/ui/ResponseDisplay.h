#pragma once

#include "FrequencyAxis.h"
#include "ResponseCurves.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

class ParametricEQAudioProcessor;

//==============================================================================
/** The log-frequency / dB display: grid, labels, each enabled band's filled
    curve, the summed curve, the dB scale on the right and the range switch.
*/
class ResponseDisplay final : public juce::Component
{
public:
    /** Space along the bottom for frequency labels, and on the right for the dB scale. */
    static constexpr int labelStripHeight = 18;
    static constexpr int scaleWidth = 40;

    explicit ResponseDisplay (ParametricEQAudioProcessor& processor);
    ~ResponseDisplay() override;

    /** Snapshots the band parameters, recomputes the curves if they changed, and
        repaints when needed. Message thread only.
    */
    void refresh();

    /** Area the curves are drawn in (inside the label strip and scale). */
    juce::Rectangle<float> getPlotArea() const;
    FrequencyAxis getAxis() const;

    const ResponseCurves& getCurves() const noexcept { return curves; }
    juce::TextButton& getRangeButton() noexcept    { return rangeButton; }

    /** The summed curve as drawn, in component coordinates. */
    juce::Path getSumPath() const;

    static juce::Colour bandColour (int bandNumber);
    static juce::Colour sumColour();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void cycleRange();
    void updateRangeButton();

    ParametricEQAudioProcessor& processor;
    ResponseCurves curves;
    juce::TextButton rangeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResponseDisplay)
};
