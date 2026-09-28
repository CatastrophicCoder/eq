#pragma once

#include <juce_graphics/juce_graphics.h>

#include <array>
#include <vector>

//==============================================================================
/** Maps frequency (log, 20 Hz - 20 kHz) to x and gain (linear, +-range) to y inside
    a plot area, and lists grid lines and labels.
*/
class FrequencyAxis
{
public:
    static constexpr double minHz = 20.0, maxHz = 20000.0;

    /** The display ranges the range switch cycles through, in dB (+- this value). */
    static constexpr std::array<double, 4> ranges { 3.0, 6.0, 12.0, 30.0 };
    static constexpr double defaultRangeDb = 12.0;

    FrequencyAxis (juce::Rectangle<float> plotArea, double rangeDb);

    float xForFrequency (double frequencyHz) const noexcept;
    double frequencyForX (float x) const noexcept;
    float yForDb (double db) const noexcept;
    double dbForY (float y) const noexcept;

    double getRangeDb() const noexcept { return range; }
    juce::Rectangle<float> getPlotArea() const noexcept { return area; }

    /** 20, 30, ... 90, 100, 200, ... 20 kHz: vertical grid lines. */
    static std::vector<double> gridFrequencies();

    /** Frequencies that get a label: 20, 50, 100, 200, 500, 1k, 2k, 5k, 10k, 20k. */
    static std::vector<double> labelledFrequencies();

    /** "20", "500", "1k", "20k". */
    static juce::String frequencyLabel (double frequencyHz);

    /** Horizontal grid lines for this range, from -range to +range. */
    std::vector<double> gridDecibels() const;

    /** Grid step for a range: 3 -> 1 dB, 6 -> 2, 12 -> 3, 30 -> 10. */
    static double gridStepDb (double rangeDb) noexcept;

    /** True for one of the values in ranges. */
    static bool isValidRange (double rangeDb) noexcept;

private:
    juce::Rectangle<float> area;
    double range;
};
