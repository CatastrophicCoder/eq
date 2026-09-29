#pragma once

#include "BandParameterWriter.h"
#include "FrequencyAxis.h"
#include "NodeDragController.h"
#include "NodeLayout.h"
#include "PeakFinder.h"
#include "ResponseCurves.h"
#include "LevelMeter.h"
#include "SelectionModel.h"
#include "SpectrumAnalyzer.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

class ParametricEQAudioProcessor;

//==============================================================================
/** The log-frequency / dB display: grid, labels, each enabled band's filled
    curve, the summed curve, the dB scale on the right and the range switch.

    Interactive (M4): a node per band in use (grey when disabled). Click selects,
    Cmd-click toggles, dragging empty space draws a selection rectangle (Shift
    adds to the selection). Dragging a node moves every selected enabled band
    (Shift: fine). Wheel or pinch changes Q. Double-click on empty space adds a
    Bell in a free slot; on a node it toggles enable/disable. Right-click opens
    the type/slope/enable/delete menu. Delete or Backspace frees the selected
    bands. Disabled bands are not editable until enabled. Every edit is a host
    gesture.
*/
class ResponseDisplay final : public juce::Component
{
public:
    /** Space along the bottom for frequency labels, and on the right for the dB scale. */
    static constexpr int labelStripHeight = 18;
    static constexpr int eqScaleWidth = 40;         // EQ dB labels
    static constexpr int analyzerScaleWidth = 36;   // analyzer dB labels
    static constexpr int meterWidth = 24;           // output meter
    static constexpr int scaleWidth = eqScaleWidth + analyzerScaleWidth + meterWidth;

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

    /** The summed curve as drawn (the only sum, or L / M), in component coordinates. */
    juce::Path getSumPath() const;
    /** The second sum (R / S) when channel modes split the display. */
    juce::Path getSecondSumPath() const;
    static juce::Colour secondSumColour();

    static juce::Colour bandColour (int bandNumber);
    static juce::Colour sumColour();
    static juce::Colour disabledColour();   // disabled bands: grey
    static juce::Colour preAnalyzerColour();    // input spectrum
    static juce::Colour postAnalyzerColour();   // output spectrum

    //==============================================================================
    // Interaction entry points. The mouse and key callbacks forward here; tests call them directly.
    void handlePress (juce::Point<float> position, juce::ModifierKeys mods, int numClicks);
    void handleDrag (juce::Point<float> position, juce::ModifierKeys mods);
    void handleRelease();
    void handleWheel (juce::Point<float> position, float deltaY);
    void handleMagnify (juce::Point<float> position, float scaleFactor);
    bool handleKey (const juce::KeyPress& key);

    /** Right-click menu for a node, and applying the chosen item (to the whole selection
        if the node is part of it). */
    juce::PopupMenu buildNodeMenu (int band) const;
    void applyNodeMenuResult (int band, int itemId);

    static constexpr int menuTypeBase = 1;      // + FilterType index
    static constexpr int menuSlopeBase = 100;   // + slope index
    static constexpr int menuChannelBase = 200;   // + ChannelMode index
    static constexpr int menuToggleEnable = 1000;
    static constexpr int menuDelete = 1001;

    std::vector<NodeLayout::Node> getNodes() const;
    const SelectionModel& getSelection() const noexcept { return selection; }
    void setSelection (std::vector<int> bands, int primary);

    /** Short-lived notice shown on the display (e.g. all bands in use); empty when none. */
    juce::String getMessage() const;

    bool isSelectingArea() const noexcept { return selectingArea; }

    /** Peak pick (M9c): the pointer over empty space shows a marker on the nearest spectrum
        peak; pressing on it creates a band there and the drag sets its gain. */
    void handleHover (juce::Point<float> position);
    std::optional<PeakFinder::Peak> getPeakMarker() const { return peakMarker; }
    juce::Point<float> getPeakMarkerPosition() const;
    int getHoveredBand() const noexcept { return hovered; }

    //==============================================================================
    /** Pulls the analyzer taps, updates the analyzers and the meter, and repaints (M5). */
    void refreshAnalyzer (double elapsedSeconds);

    /** Applies stored analyzer settings (resolution, speed) and freeze. */
    void setAnalyzerFrozen (bool frozen);

    /** Area of the output meter (clicking it resets the clip lights). */
    juce::Rectangle<float> getMeterArea() const;

    SpectrumAnalyzer& getPreAnalyzer() noexcept  { return preAnalyzer; }
    SpectrumAnalyzer& getPostAnalyzer() noexcept { return postAnalyzer; }
    LevelMeter& getMeter() noexcept              { return meter; }

    /** Called whenever the selection changes. */
    std::function<void()> onSelectionChanged;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    bool keyPressed (const juce::KeyPress&) override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void cycleRange();
    void updateRangeButton();

    std::array<BandSettings, ResponseCurves::numBands> currentBands() const;
    void selectionChanged();
    void addBandAt (juce::Point<float> position);
    void deleteBands (const std::vector<int>& bands);
    void setEnabled (const std::vector<int>& bands, bool enabled);
    void scaleQ (juce::Point<float> position, double factor);
    void showMessage (const juce::String& text);
    void paintNodes (juce::Graphics&, const FrequencyAxis&);

    ParametricEQAudioProcessor& processor;
    BandParameterWriter writer;
    ResponseCurves curves;
    juce::TextButton rangeButton;

    SelectionModel selection;
    NodeDragController drag;
    bool selectingArea = false, extendSelection = false;
    juce::Point<float> areaStart, areaEnd;
    std::vector<int> selectionBeforeArea;
    int hovered = 0;
    std::optional<PeakFinder::Peak> peakMarker;

    juce::String message;
    juce::uint32 messageTime = 0;

    void paintAnalyzer (juce::Graphics&, const FrequencyAxis&);
    void paintMeter (juce::Graphics&);

    SpectrumAnalyzer preAnalyzer, postAnalyzer;
    LevelMeter meter;
    juce::AudioBuffer<float> tapScratch { 2, 4096 };
    std::vector<float> monoScratch = std::vector<float> (4096);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResponseDisplay)
};
