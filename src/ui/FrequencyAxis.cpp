#include "FrequencyAxis.h"

#include <cmath>

FrequencyAxis::FrequencyAxis (juce::Rectangle<float> plotArea, double rangeDb)
    : area (plotArea), range (rangeDb)
{
}

float FrequencyAxis::xForFrequency (double frequencyHz) const noexcept
{
    const auto proportion = std::log (frequencyHz / minHz) / std::log (maxHz / minHz);
    return area.getX() + static_cast<float> (proportion) * area.getWidth();
}

double FrequencyAxis::frequencyForX (float x) const noexcept
{
    const auto proportion = static_cast<double> ((x - area.getX()) / area.getWidth());
    return minHz * std::pow (maxHz / minHz, proportion);
}

float FrequencyAxis::yForDb (double db) const noexcept
{
    // +range at the top, -range at the bottom.
    const auto proportion = 0.5 - 0.5 * db / range;
    return area.getY() + static_cast<float> (proportion) * area.getHeight();
}

double FrequencyAxis::dbForY (float y) const noexcept
{
    const auto proportion = static_cast<double> ((y - area.getY()) / area.getHeight());
    return (0.5 - proportion) * 2.0 * range;
}

std::vector<double> FrequencyAxis::gridFrequencies()
{
    std::vector<double> result;

    for (double decade = 10.0; decade <= 10000.0; decade *= 10.0)
        for (int m = 1; m <= 9; ++m)
            if (const auto f = m * decade; f >= minHz && f <= maxHz)
                result.push_back (f);

    result.push_back (maxHz);
    return result;
}

std::vector<double> FrequencyAxis::labelledFrequencies()
{
    return { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
}

juce::String FrequencyAxis::frequencyLabel (double frequencyHz)
{
    if (frequencyHz >= 1000.0)
        return juce::String (juce::roundToInt (frequencyHz / 1000.0)) + "k";

    return juce::String (juce::roundToInt (frequencyHz));
}

std::vector<double> FrequencyAxis::gridDecibels() const
{
    std::vector<double> result;
    const auto step = gridStepDb (range);
    const auto steps = juce::roundToInt (range / step);

    for (int i = -steps; i <= steps; ++i)
        result.push_back (i * step);

    return result;
}

double FrequencyAxis::gridStepDb (double rangeDb) noexcept
{
    if (rangeDb <= 3.0)  return 1.0;
    if (rangeDb <= 6.0)  return 2.0;
    if (rangeDb <= 12.0) return 3.0;
    return 10.0;
}

bool FrequencyAxis::isValidRange (double rangeDb) noexcept
{
    for (auto r : ranges)
        if (juce::exactlyEqual (r, rangeDb))
            return true;

    return false;
}
