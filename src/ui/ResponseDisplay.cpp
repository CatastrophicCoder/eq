#include "ResponseDisplay.h"

#include "Parameters.h"
#include "PluginProcessor.h"
#include "SpectrumColour.h"
#include "AnalyzerSettings.h"
#include "dsp/CutSlope.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Our own colour values (decision 2026-09-28: similar in character to the reference, not sampled from it).
    const juce::Colour background   { 0xff17171d };
    const juce::Colour gridLine     { 0x10ffffff };
    const juce::Colour gridStrong   { 0x22ffffff };
    const juce::Colour labelColour  { 0xff8b8a96 };

    constexpr float topMargin = 6.0f;
}

ResponseDisplay::ResponseDisplay (ParametricEQAudioProcessor& p)
    : processor (p), writer (p.getValueTreeState())
{
    setWantsKeyboardFocus (true);
    setName ("display");
    rangeButton.setName ("range");
    rangeButton.onClick = [this] { cycleRange(); };
    addAndMakeVisible (rangeButton);
    updateRangeButton();
}

ResponseDisplay::~ResponseDisplay() = default;

juce::Colour ResponseDisplay::bandColour (int bandNumber)
{
    // Visible spectrum from band 1 (dark violet) to band 16 (red), evenly spaced in wavelength.
    return SpectrumColour::fromWavelength (SpectrumColour::wavelengthForBand (bandNumber, ResponseCurves::numBands));
}

juce::Colour ResponseDisplay::disabledColour()
{
    return juce::Colour { 0xff7a7a82 };
}

juce::Colour ResponseDisplay::sumColour()
{
    return juce::Colour { 0xfff0b43c };   // amber
}

void ResponseDisplay::refresh()
{
    const auto bands = currentBands();

    // Bands deleted elsewhere leave the selection; disabled ones stay selected.
    auto pruned = false;
    for (auto b : std::vector<int> (selection.getSelected()))
        if (! bands[static_cast<size_t> (b - 1)].inUse)
        {
            selection.remove (b);
            pruned = true;
        }

    if (pruned)
        selectionChanged();

    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const auto rangeChanged = rangeButton.getButtonText() != juce::String (juce::roundToInt (processor.getDisplayRangeDb())) + " dB";

    if (rangeChanged)
        updateRangeButton();

    if (curves.update (bands, rate) || rangeChanged)
        repaint();
}

juce::Rectangle<float> ResponseDisplay::getPlotArea() const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromTop (topMargin);
    area.removeFromBottom (static_cast<float> (labelStripHeight));
    area.removeFromRight (static_cast<float> (scaleWidth));
    return area;
}

FrequencyAxis ResponseDisplay::getAxis() const
{
    return { getPlotArea(), processor.getDisplayRangeDb() };
}

namespace
{
    /** Curve as a path, with y held inside the plot so extreme settings cannot run off the display. */
    template <typename DbAt>
    juce::Path curvePath (const FrequencyAxis& axis, const ResponseCurves& curves, DbAt&& dbAt)
    {
        const auto plot = axis.getPlotArea();
        juce::Path path;

        for (int k = 0; k < ResponseCurves::numPoints; ++k)
        {
            const auto x = axis.xForFrequency (curves.frequency (k));
            const auto y = juce::jlimit (plot.getY(), plot.getBottom(), axis.yForDb (dbAt (k)));

            if (k == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        return path;
    }
}

juce::Path ResponseDisplay::getSumPath() const
{
    return curvePath (getAxis(), curves, [this] (int k) { return curves.sumDb (k); });
}

void ResponseDisplay::paint (juce::Graphics& g)
{
    g.fillAll (background);

    const auto axis = getAxis();
    const auto plot = axis.getPlotArea();

    // Grid.
    const auto labelled = FrequencyAxis::labelledFrequencies();
    for (auto f : FrequencyAxis::gridFrequencies())
    {
        const auto isLabelled = std::find (labelled.begin(), labelled.end(), f) != labelled.end();
        g.setColour (isLabelled ? gridStrong : gridLine);
        g.drawVerticalLine (juce::roundToInt (axis.xForFrequency (f)), plot.getY(), plot.getBottom());
    }

    for (auto db : axis.gridDecibels())
    {
        g.setColour (juce::exactlyEqual (db, 0.0) ? gridStrong.withMultipliedAlpha (1.6f) : gridLine);
        g.drawHorizontalLine (juce::roundToInt (axis.yForDb (db)), plot.getX(), plot.getRight());
    }

    paintAnalyzer (g, axis);

    // Frequency labels along the bottom, dB scale on the right.
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (labelColour);
    for (auto f : labelled)
    {
        const auto x = juce::roundToInt (axis.xForFrequency (f));
        const auto justification = f <= FrequencyAxis::minHz ? juce::Justification::centredLeft
                                 : f >= FrequencyAxis::maxHz ? juce::Justification::centredRight
                                                             : juce::Justification::centred;
        const auto labelX = f <= FrequencyAxis::minHz ? x + 2 : f >= FrequencyAxis::maxHz ? x - 42 : x - 20;
        g.drawText (FrequencyAxis::frequencyLabel (f), labelX, getHeight() - labelStripHeight, 40, labelStripHeight, justification);
    }

    g.setColour (sumColour().withMultipliedAlpha (0.85f));
    for (auto db : axis.gridDecibels())
    {
        // Keep the label box inside the display, so the top and bottom values are not clipped.
        const auto y = juce::jlimit (7, getHeight() - labelStripHeight - 7, juce::roundToInt (axis.yForDb (db)));
        const auto text = db > 0.0 ? "+" + juce::String (juce::roundToInt (db)) : juce::String (juce::roundToInt (db));
        g.drawText (text, juce::roundToInt (plot.getRight()) + 4, y - 7, eqScaleWidth - 8, 14, juce::Justification::centredRight);
    }

    // Each active band: filled between its curve and 0 dB, then its outline.
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (plot.toNearestInt());
        const auto zeroY = axis.yForDb (0.0);

        for (int b = 0; b < ResponseCurves::numBands; ++b)
        {
            if (! curves.isBandShown (b))
                continue;

            auto outline = curvePath (axis, curves, [this, b] (int k) { return curves.bandDb (b, k); });

            // Disabled: grey outline only, no fill (and not part of the sum).
            if (! curves.isBandActive (b))
            {
                g.setColour (disabledColour().withAlpha (0.6f));
                g.strokePath (outline, juce::PathStrokeType (1.0f));
                continue;
            }

            auto fill = outline;
            fill.lineTo (axis.xForFrequency (FrequencyAxis::maxHz), zeroY);
            fill.lineTo (axis.xForFrequency (FrequencyAxis::minHz), zeroY);
            fill.closeSubPath();

            const auto colour = bandColour (b + 1);
            g.setColour (colour.withAlpha (0.22f));
            g.fillPath (fill);
            g.setColour (colour.withAlpha (0.75f));
            g.strokePath (outline, juce::PathStrokeType (1.2f));
        }

        g.setColour (sumColour());
        g.strokePath (getSumPath(), juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    paintNodes (g, axis);
    paintMeter (g);
}

void ResponseDisplay::resized()
{
    const auto plot = getPlotArea().toNearestInt();
    rangeButton.setBounds (plot.getRight() - 58, plot.getY() + 4, 54, 20);
}

void ResponseDisplay::cycleRange()
{
    const auto current = processor.getDisplayRangeDb();
    auto next = FrequencyAxis::ranges.front();

    for (size_t i = 0; i < FrequencyAxis::ranges.size(); ++i)
        if (juce::exactlyEqual (FrequencyAxis::ranges[i], current))
            next = FrequencyAxis::ranges[(i + 1) % FrequencyAxis::ranges.size()];

    processor.setDisplayRangeDb (next);
    updateRangeButton();
    repaint();
}

void ResponseDisplay::updateRangeButton()
{
    rangeButton.setButtonText (juce::String (juce::roundToInt (processor.getDisplayRangeDb())) + " dB");
}

//==============================================================================
// Interaction (M4).

std::array<BandSettings, ResponseCurves::numBands> ResponseDisplay::currentBands() const
{
    return processor.getBandSettings();
}

std::vector<NodeLayout::Node> ResponseDisplay::getNodes() const
{
    return NodeLayout::compute (currentBands(), getAxis());
}

void ResponseDisplay::setSelection (std::vector<int> bands, int primary)
{
    selection.set (std::move (bands), primary);
    selectionChanged();
}

void ResponseDisplay::selectionChanged()
{
    repaint();
    if (onSelectionChanged != nullptr)
        onSelectionChanged();
}

void ResponseDisplay::handlePress (juce::Point<float> position, juce::ModifierKeys mods, int numClicks)
{
    const auto nodes = getNodes();
    const auto band = NodeLayout::bandAt (nodes, position);

    if (numClicks >= 2)
    {
        if (band != 0)
            setEnabled ({ band }, ! currentBands()[static_cast<size_t> (band - 1)].enabled);
        else
            addBandAt (position);
        return;
    }

    if (band == 0)
    {
        // Empty space: start a selection rectangle (Shift keeps what is selected).
        extendSelection = mods.isShiftDown();
        selectionBeforeArea = extendSelection ? selection.getSelected() : std::vector<int> {};
        if (! extendSelection)
        {
            selection.clear();
            selectionChanged();
        }

        selectingArea = true;
        areaStart = areaEnd = position;
        return;
    }

    if (mods.isCommandDown())
    {
        selection.toggle (band);
        selectionChanged();
        if (! selection.contains (band))
            return;   // toggled off: nothing to drag
    }
    else if (! selection.contains (band))
    {
        selection.select (band);
        selectionChanged();
    }
    else
    {
        selection.set (selection.getSelected(), band);   // keep the selection, make this band primary
        selectionChanged();
    }

    // Drag every selected enabled band, inside one gesture per parameter. Disabled bands stay put.
    const auto bands = currentBands();
    std::vector<NodeDragController::BandStart> starts;

    for (auto b : selection.getSelected())
    {
        const auto& settings = bands[static_cast<size_t> (b - 1)];
        if (! settings.isActive())
            continue;

        const auto usesGain = FilterTypes::usesGain (settings.type);
        starts.push_back ({ b, settings.frequencyHz, settings.gainDb, usesGain });

        writer.beginGesture (b, "freq");
        if (usesGain)
            writer.beginGesture (b, "gain");
    }

    drag.begin (std::move (starts), position);
}

void ResponseDisplay::handleDrag (juce::Point<float> position, juce::ModifierKeys mods)
{
    if (drag.isActive())
    {
        for (const auto& v : drag.dragTo (position, mods.isShiftDown(), getAxis()))
        {
            writer.set (v.band, "freq", static_cast<float> (v.frequencyHz));

            const auto type = currentBands()[static_cast<size_t> (v.band - 1)].type;
            if (FilterTypes::usesGain (type))
                writer.set (v.band, "gain", static_cast<float> (v.gainDb));
        }

        refresh();
        return;
    }

    if (selectingArea)
    {
        areaEnd = position;
        auto chosen = NodeLayout::bandsIn (getNodes(), juce::Rectangle<float> (areaStart, areaEnd));

        if (extendSelection)
            chosen.insert (chosen.end(), selectionBeforeArea.begin(), selectionBeforeArea.end());

        selection.set (chosen, selection.getPrimary());
        selectionChanged();
    }
}

void ResponseDisplay::handleRelease()
{
    if (drag.isActive())
    {
        for (const auto& s : drag.getBands())
        {
            writer.endGesture (s.band, "freq");
            if (s.usesGain)
                writer.endGesture (s.band, "gain");
        }

        drag.end();
    }

    if (selectingArea)
    {
        selectingArea = false;
        repaint();
    }
}

void ResponseDisplay::scaleQ (juce::Point<float> position, double factor)
{
    // The node under the pointer (with its selection, if it is part of one), else the selection.
    const auto band = NodeLayout::bandAt (getNodes(), position);
    std::vector<int> targets;

    if (band != 0)
        targets = selection.contains (band) ? selection.getSelected() : std::vector<int> { band };
    else
        targets = selection.getSelected();

    const auto bands = currentBands();
    for (auto b : targets)
    {
        const auto& settings = bands[static_cast<size_t> (b - 1)];
        if (settings.isActive() && FilterTypes::usesQ (settings.type))
            writer.setOnce (b, "q", static_cast<float> (NodeDragController::scaleQ (settings.q, factor)));
    }

    refresh();
}

void ResponseDisplay::handleWheel (juce::Point<float> position, float deltaY)
{
    scaleQ (position, NodeDragController::wheelFactor (deltaY));
}

void ResponseDisplay::handleMagnify (juce::Point<float> position, float scaleFactor)
{
    scaleQ (position, static_cast<double> (scaleFactor));
}

bool ResponseDisplay::handleKey (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::deleteKey) || key.isKeyCode (juce::KeyPress::backspaceKey))
    {
        if (selection.isEmpty())
            return false;

        deleteBands (selection.getSelected());
        return true;
    }

    return false;   // everything else goes to the host (transport, shortcuts)
}

void ResponseDisplay::addBandAt (juce::Point<float> position)
{
    const auto bands = currentBands();
    int free = 0;

    for (int b = 1; b <= ResponseCurves::numBands && free == 0; ++b)
        if (! bands[static_cast<size_t> (b - 1)].inUse)
            free = b;

    if (free == 0)
    {
        showMessage ("All 16 bands in use");
        return;
    }

    const auto axis = getAxis();
    const auto f = std::clamp (axis.frequencyForX (position.x), NodeDragController::minFrequency, NodeDragController::maxFrequency);
    const auto g = std::clamp (axis.dbForY (position.y), NodeDragController::minGain, NodeDragController::maxGain);

    writer.setOnce (free, "type", static_cast<float> (FilterType::bell));
    writer.setOnce (free, "freq", static_cast<float> (f));
    writer.setOnce (free, "gain", static_cast<float> (g));
    writer.setOnce (free, "q", 1.0f);
    writer.setOnce (free, "enabled", 1.0f);
    processor.setBandInUse (free, true);

    selection.select (free);
    selectionChanged();
    refresh();
}

void ResponseDisplay::deleteBands (const std::vector<int>& bands)
{
    const auto targets = bands;   // copy: the selection may be what we were given

    for (auto b : targets)
    {
        writer.setOnce (b, "enabled", 0.0f);
        processor.setBandInUse (b, false);
        selection.remove (b);
    }

    selectionChanged();
    refresh();
}

void ResponseDisplay::setEnabled (const std::vector<int>& bands, bool enabled)
{
    for (auto b : bands)
        writer.setOnce (b, "enabled", enabled ? 1.0f : 0.0f);

    repaint();
    refresh();
}

juce::PopupMenu ResponseDisplay::buildNodeMenu (int band) const
{
    const auto settings = currentBands()[static_cast<size_t> (band - 1)];
    const auto on = settings.enabled;   // a disabled band: only Enable and Delete are available
    juce::PopupMenu menu;

    for (int t = 0; t < FilterTypes::count; ++t)
        menu.addItem (menuTypeBase + t, FilterTypes::names[t], on, static_cast<int> (settings.type) == t);

    if (FilterTypes::usesSlope (settings.type))
    {
        juce::PopupMenu slopes;
        for (int sl = 0; sl < CutSlope::count; ++sl)
            slopes.addItem (menuSlopeBase + sl, CutSlope::labels[sl], on, settings.slopeIndex == sl);
        menu.addSeparator();
        menu.addSubMenu ("Slope", slopes, on);
    }

    menu.addSeparator();
    menu.addItem (menuToggleEnable, on ? "Disable" : "Enable");
    menu.addItem (menuDelete, "Delete");
    return menu;
}

void ResponseDisplay::applyNodeMenuResult (int band, int itemId)
{
    if (itemId <= 0)
        return;   // menu dismissed

    const auto targets = selection.contains (band) ? selection.getSelected() : std::vector<int> { band };
    const auto bands = currentBands();

    if (itemId == menuDelete)
    {
        deleteBands (targets);
        return;
    }

    if (itemId == menuToggleEnable)
    {
        // Every target follows the clicked band: "Disable" disables them all, "Enable" enables them all.
        setEnabled (targets, ! bands[static_cast<size_t> (band - 1)].enabled);
        return;
    }

    // Type and slope changes skip disabled bands.
    for (auto b : targets)
    {
        if (! bands[static_cast<size_t> (b - 1)].enabled)
            continue;

        if (itemId >= menuSlopeBase && itemId < menuSlopeBase + CutSlope::count)
            writer.setOnce (b, "slope", static_cast<float> (itemId - menuSlopeBase));
        else if (itemId >= menuTypeBase && itemId < menuTypeBase + FilterTypes::count)
            writer.setOnce (b, "type", static_cast<float> (itemId - menuTypeBase));
    }

    refresh();
}

void ResponseDisplay::showMessage (const juce::String& text)
{
    message = text;
    messageTime = juce::Time::getMillisecondCounter();
    repaint();
}

juce::String ResponseDisplay::getMessage() const
{
    constexpr juce::uint32 messageDurationMs = 2000;
    return juce::Time::getMillisecondCounter() - messageTime < messageDurationMs ? message : juce::String();
}

//==============================================================================
void ResponseDisplay::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    if (getMeterArea().contains (e.position))
    {
        meter.resetClip();
        repaint();
        return;
    }

    if (e.mods.isPopupMenu())
    {
        const auto band = NodeLayout::bandAt (getNodes(), e.position);
        if (band == 0)
            return;

        if (! selection.contains (band))
        {
            selection.select (band);
            selectionChanged();
        }

        juce::Component::SafePointer<ResponseDisplay> safe (this);
        buildNodeMenu (band).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                                          .withMousePosition(),
                                            [safe, band] (int result)
                                            {
                                                if (safe != nullptr)
                                                    safe->applyNodeMenuResult (band, result);
                                            });
        return;
    }

    handlePress (e.position, e.mods, e.getNumberOfClicks());
}

void ResponseDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        handleDrag (e.position, e.mods);
}

void ResponseDisplay::mouseUp (const juce::MouseEvent&)
{
    handleRelease();
}

void ResponseDisplay::mouseMove (const juce::MouseEvent& e)
{
    const auto band = NodeLayout::bandAt (getNodes(), e.position);
    if (band != hovered)
    {
        hovered = band;
        repaint();
    }
}

void ResponseDisplay::mouseExit (const juce::MouseEvent&)
{
    if (hovered != 0)
    {
        hovered = 0;
        repaint();
    }
}

void ResponseDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    handleWheel (e.position, wheel.deltaY);
}

void ResponseDisplay::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    handleMagnify (e.position, scaleFactor);
}

bool ResponseDisplay::keyPressed (const juce::KeyPress& key)
{
    return handleKey (key);
}

//==============================================================================
void ResponseDisplay::refreshAnalyzer (double elapsedSeconds)
{
    const auto settings = processor.getAnalyzerSettings();
    const auto mode = static_cast<AnalyzerSettings::Mode> (settings.mode);
    const auto showPre = mode == AnalyzerSettings::Mode::pre || mode == AnalyzerSettings::Mode::prePost;
    const auto showPost = mode == AnalyzerSettings::Mode::post || mode == AnalyzerSettings::Mode::prePost;
    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    for (auto* a : { &preAnalyzer, &postAnalyzer })
    {
        a->setFftOrder (AnalyzerSettings::fftOrders[static_cast<size_t> (settings.resolution)]);
        a->setReleaseDbPerSecond (AnalyzerSettings::releaseDbPerSecond[static_cast<size_t> (settings.speed)]);
    }

    // Drain both taps every time, so they never fill up; feed only what is shown.
    auto drain = [&] (AnalyzerFifo& fifo, SpectrumAnalyzer* analyzer, bool feedMeter)
    {
        for (int n = fifo.pull (tapScratch); n > 0; n = fifo.pull (tapScratch))
        {
            const auto* left = tapScratch.getReadPointer (0);
            const auto* right = tapScratch.getReadPointer (1);

            if (feedMeter)
                meter.addSamples (left, right, n, rate);

            if (analyzer != nullptr)
            {
                for (int i = 0; i < n; ++i)
                    monoScratch[static_cast<size_t> (i)] = 0.5f * (left[i] + right[i]);
                analyzer->addSamples (monoScratch.data(), n);
            }
        }
    };

    drain (processor.getPreFifo(), showPre ? &preAnalyzer : nullptr, false);
    drain (processor.getPostFifo(), showPost ? &postAnalyzer : nullptr, true);

    if (showPre)
        preAnalyzer.update (rate, elapsedSeconds);
    if (showPost)
        postAnalyzer.update (rate, elapsedSeconds);

    meter.update (elapsedSeconds);
    repaint();
}

void ResponseDisplay::setAnalyzerFrozen (bool frozen)
{
    preAnalyzer.setFrozen (frozen);
    postAnalyzer.setFrozen (frozen);
}

juce::Rectangle<float> ResponseDisplay::getMeterArea() const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromTop (6.0f);
    area.removeFromBottom (static_cast<float> (labelStripHeight));
    return area.removeFromRight (static_cast<float> (meterWidth)).reduced (3.0f, 0.0f);
}

void ResponseDisplay::paintAnalyzer (juce::Graphics& g, const FrequencyAxis& axis)
{
    const auto settings = processor.getAnalyzerSettings();
    const auto mode = static_cast<AnalyzerSettings::Mode> (settings.mode);
    if (mode == AnalyzerSettings::Mode::off)
        return;

    const auto range = AnalyzerSettings::ranges[static_cast<size_t> (settings.range)];
    const auto plot = axis.getPlotArea();
    auto yFor = [&] (double db) { return juce::jlimit (plot.getY(), plot.getBottom(), plot.getY() + static_cast<float> (-db / range) * plot.getHeight()); };

    auto draw = [&] (const SpectrumAnalyzer& a, float fillAlpha, float lineAlpha)
    {
        juce::Path path;
        path.startNewSubPath (axis.xForFrequency (a.frequency (0)), plot.getBottom());
        for (int k = 0; k < SpectrumAnalyzer::numPoints; ++k)
            path.lineTo (axis.xForFrequency (a.frequency (k)), yFor (a.displayDb (k)));
        path.lineTo (axis.xForFrequency (a.frequency (SpectrumAnalyzer::numPoints - 1)), plot.getBottom());
        path.closeSubPath();

        const auto colour = juce::Colour { 0xffb8c4d6 };
        g.setColour (colour.withAlpha (fillAlpha));
        g.fillPath (path);
        if (lineAlpha > 0.0f)
        {
            g.setColour (colour.withAlpha (lineAlpha));
            g.strokePath (path, juce::PathStrokeType (1.0f));
        }
    };

    if (mode == AnalyzerSettings::Mode::pre || mode == AnalyzerSettings::Mode::prePost)
        draw (preAnalyzer, 0.07f, 0.0f);         // input: faint
    if (mode == AnalyzerSettings::Mode::post || mode == AnalyzerSettings::Mode::prePost)
        draw (postAnalyzer, 0.14f, 0.35f);       // output: brighter

    // Analyzer dB scale, in its own column.
    const auto step = range <= 60.0 ? 10.0 : range <= 90.0 ? 15.0 : 20.0;
    g.setFont (juce::FontOptions (10.0f));
    g.setColour (juce::Colour { 0xff6f6e7a });
    const auto x = juce::roundToInt (plot.getRight()) + eqScaleWidth;
    for (double db = 0.0; db >= -range - 1e-9; db -= step)
    {
        const auto y = juce::jlimit (7, getHeight() - labelStripHeight - 7, juce::roundToInt (yFor (db)));
        g.drawText (juce::String (juce::roundToInt (db)), x, y - 6, analyzerScaleWidth - 6, 12, juce::Justification::centredRight);
    }
}

void ResponseDisplay::paintMeter (juce::Graphics& g)
{
    // -60 to 0 dBFS; RMS as a bar, peak as a line, clip light on top.
    constexpr double meterFloor = -60.0;
    const auto area = getMeterArea();
    const auto clipHeight = 5.0f;
    auto bars = area.withTrimmedTop (clipHeight + 2.0f);
    const auto barWidth = (bars.getWidth() - 2.0f) / 2.0f;

    auto yFor = [&] (double db)
    {
        const auto t = static_cast<float> (juce::jlimit (0.0, 1.0, (db - meterFloor) / -meterFloor));
        return bars.getBottom() - t * bars.getHeight();
    };

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto x = bars.getX() + static_cast<float> (ch) * (barWidth + 2.0f);
        const auto column = juce::Rectangle<float> (x, bars.getY(), barWidth, bars.getHeight());

        g.setColour (juce::Colour { 0xff101014 });
        g.fillRect (column);

        const auto rmsY = yFor (meter.rmsDb (ch));
        g.setGradientFill (juce::ColourGradient (juce::Colour { 0xff3fbf6f }, 0.0f, column.getBottom(),
                                                 juce::Colour { 0xffe0c040 }, 0.0f, column.getY(), false));
        g.fillRect (column.withTop (rmsY));

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (x, yFor (meter.peakDb (ch)) - 1.0f, barWidth, 2.0f));

        g.setColour (meter.isClipped (ch) ? juce::Colour { 0xffe03a3a } : juce::Colour { 0xff2a2a33 });
        g.fillRect (juce::Rectangle<float> (x, area.getY(), barWidth, clipHeight));
    }
}

void ResponseDisplay::paintNodes (juce::Graphics& g, const FrequencyAxis& axis)
{
    const auto bands = currentBands();

    for (const auto& n : NodeLayout::compute (bands, axis))
    {
        const auto colour = n.enabled ? bandColour (n.band) : disabledColour();
        const auto isSelected = selection.contains (n.band);
        const auto radius = (n.band == hovered || isSelected) ? 7.5f : 6.0f;

        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (n.position));

        if (isSelected)
        {
            g.setColour (juce::Colours::white);
            g.drawEllipse (juce::Rectangle<float> (radius * 2.0f + 4.0f, radius * 2.0f + 4.0f).withCentre (n.position),
                           n.band == selection.getPrimary() ? 2.0f : 1.2f);
        }
    }

    // Readout for the hovered band, else the primary one.
    const auto shown = hovered != 0 ? hovered : selection.getPrimary();
    if (shown != 0 && bands[static_cast<size_t> (shown - 1)].inUse)
    {
        const auto& b = bands[static_cast<size_t> (shown - 1)];
        auto text = (b.frequencyHz >= 1000.0 ? juce::String (b.frequencyHz / 1000.0, 2) + " kHz"
                                             : juce::String (juce::roundToInt (b.frequencyHz)) + " Hz");
        if (FilterTypes::usesGain (b.type))
            text << "   " << (b.gainDb > 0.0 ? "+" : "") << juce::String (b.gainDb, 1) << " dB";
        if (FilterTypes::usesQ (b.type))
            text << "   Q " << juce::String (b.q, 2);

        for (const auto& n : NodeLayout::compute (bands, axis))
            if (n.band == shown)
            {
                const auto box = juce::Rectangle<float> (160.0f, 20.0f)
                                     .withCentre (n.position.translated (0.0f, -24.0f))
                                     .constrainedWithin (axis.getPlotArea());
                g.setColour (juce::Colour { 0xd0101014 });
                g.fillRoundedRectangle (box, 4.0f);
                g.setColour (b.enabled ? bandColour (shown) : disabledColour());
                g.setFont (juce::FontOptions (12.0f));
                g.drawText (text, box, juce::Justification::centred);
            }
    }

    if (selectingArea)
    {
        const auto area = juce::Rectangle<float> (areaStart, areaEnd);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (area);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.drawRect (area, 1.0f);
    }

    if (const auto text = getMessage(); text.isNotEmpty())
    {
        const auto box = juce::Rectangle<float> (220.0f, 28.0f).withCentre (axis.getPlotArea().getCentre().withY (axis.getPlotArea().getY() + 40.0f));
        g.setColour (juce::Colour { 0xe0101014 });
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (text, box, juce::Justification::centred);
    }
}
