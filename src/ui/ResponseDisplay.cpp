#include "ResponseDisplay.h"

#include "Parameters.h"
#include "PluginProcessor.h"
#include "SpectrumColour.h"

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
    : processor (p)
{
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

juce::Colour ResponseDisplay::sumColour()
{
    return juce::Colour { 0xfff0b43c };   // amber
}

void ResponseDisplay::refresh()
{
    std::array<BandSettings, ResponseCurves::numBands> bands;
    auto& state = processor.getValueTreeState();

    for (int band = 1; band <= ResponseCurves::numBands; ++band)
    {
        auto raw = [&] (const char* field) { return state.getRawParameterValue (Parameters::id (band, field))->load(); };
        bands[static_cast<size_t> (band - 1)] = Parameters::toBandSettings (raw ("type"), raw ("freq"), raw ("gain"),
                                                                            raw ("q"), raw ("slope"), raw ("enabled"));
    }

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
        g.drawText (text, juce::roundToInt (plot.getRight()) + 4, y - 7, scaleWidth - 8, 14, juce::Justification::centredRight);
    }

    // Each active band: filled between its curve and 0 dB, then its outline.
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (plot.toNearestInt());
        const auto zeroY = axis.yForDb (0.0);

        for (int b = 0; b < ResponseCurves::numBands; ++b)
        {
            if (! curves.isBandActive (b))
                continue;

            auto outline = curvePath (axis, curves, [this, b] (int k) { return curves.bandDb (b, k); });
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
