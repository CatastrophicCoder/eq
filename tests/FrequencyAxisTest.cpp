#include "ui/FrequencyAxis.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
    const juce::Rectangle<float> area { 10.0f, 20.0f, 800.0f, 400.0f };
}

TEST_CASE ("Frequency maps logarithmically across the plot width", "[axis]")
{
    const FrequencyAxis axis (area, 12.0);

    CHECK_THAT (axis.xForFrequency (20.0), WithinAbs (10.0, 1e-4));
    CHECK_THAT (axis.xForFrequency (20000.0), WithinAbs (810.0, 1e-4));
    CHECK_THAT (axis.xForFrequency (std::sqrt (20.0 * 20000.0)), WithinAbs (410.0, 1e-3));

    // Equal ratios are equal distances.
    CHECK_THAT (axis.xForFrequency (200.0) - axis.xForFrequency (100.0),
                WithinAbs (axis.xForFrequency (8000.0) - axis.xForFrequency (4000.0), 1e-3));

    for (auto f : { 20.0, 47.0, 1000.0, 12345.0, 20000.0 })
        CHECK_THAT (axis.frequencyForX (axis.xForFrequency (f)), WithinRel (f, 1e-5));
}

TEST_CASE ("Gain maps linearly with 0 dB in the middle", "[axis]")
{
    for (auto range : FrequencyAxis::ranges)
    {
        const FrequencyAxis axis (area, range);
        INFO ("range " << range);

        CHECK_THAT (axis.yForDb (0.0), WithinAbs (220.0, 1e-4));
        CHECK_THAT (axis.yForDb (range), WithinAbs (20.0, 1e-4));    // top
        CHECK_THAT (axis.yForDb (-range), WithinAbs (420.0, 1e-4));  // bottom

        for (auto db : { -range, -0.5 * range, 0.0, 0.25 * range, range })
            CHECK_THAT (axis.dbForY (axis.yForDb (db)), WithinAbs (db, 1e-4));
    }
}

TEST_CASE ("Grid frequencies run 20, 30 ... 20 kHz", "[axis]")
{
    const auto grid = FrequencyAxis::gridFrequencies();
    REQUIRE_FALSE (grid.empty());
    CHECK_THAT (grid.front(), WithinAbs (20.0, 0.0));
    CHECK_THAT (grid.back(), WithinAbs (20000.0, 0.0));
    CHECK (std::is_sorted (grid.begin(), grid.end()));

    for (auto f : { 30.0, 90.0, 100.0, 500.0, 1000.0, 9000.0, 10000.0 })
        CHECK (std::find_if (grid.begin(), grid.end(), [f] (double g) { return std::abs (g - f) < 1e-9; }) != grid.end());

    const std::vector<double> labelled { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    CHECK (FrequencyAxis::labelledFrequencies() == labelled);
}

TEST_CASE ("Frequency labels use k for kilohertz", "[axis]")
{
    CHECK (FrequencyAxis::frequencyLabel (20.0) == "20");
    CHECK (FrequencyAxis::frequencyLabel (500.0) == "500");
    CHECK (FrequencyAxis::frequencyLabel (1000.0) == "1k");
    CHECK (FrequencyAxis::frequencyLabel (2000.0) == "2k");
    CHECK (FrequencyAxis::frequencyLabel (20000.0) == "20k");
}

TEST_CASE ("dB grid steps suit each range", "[axis]")
{
    CHECK_THAT (FrequencyAxis::gridStepDb (3.0), WithinAbs (1.0, 0.0));
    CHECK_THAT (FrequencyAxis::gridStepDb (6.0), WithinAbs (2.0, 0.0));
    CHECK_THAT (FrequencyAxis::gridStepDb (12.0), WithinAbs (3.0, 0.0));
    CHECK_THAT (FrequencyAxis::gridStepDb (30.0), WithinAbs (10.0, 0.0));

    const FrequencyAxis axis (area, 12.0);
    const std::vector<double> expected { -12, -9, -6, -3, 0, 3, 6, 9, 12 };
    CHECK (axis.gridDecibels() == expected);
}

TEST_CASE ("Only the four listed ranges are valid", "[axis]")
{
    for (auto r : FrequencyAxis::ranges)
        CHECK (FrequencyAxis::isValidRange (r));
    for (auto r : { 0.0, 5.0, 12.5, 24.0, -12.0 })
        CHECK_FALSE (FrequencyAxis::isValidRange (r));
    CHECK (FrequencyAxis::isValidRange (FrequencyAxis::defaultRangeDb));
}
