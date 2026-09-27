#include "DesignTestGrid.h"

#include "dsp/MatchedPeakingDesign.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double toleranceDb = 0.1;

    // The matched design (Vicanek 2016, section 4.4) only constrains DC and f0, so between f0
    // and Nyquist it departs from the analog curve. Worst case on the grid below is 0.317 dB
    // (-18 dB, Q 0.5, f0/fs ~ 0.11, at +1 octave). Decision 2026-09-27: bound it at 0.35 dB.
    constexpr double analogOctaveBoundDb = 0.35;

    struct Case
    {
        double sampleRate;
        double centreHz;
    };

    // f0 = 100 Hz, 1 kHz, 5 kHz at 44.1/48/96 kHz, and 10 kHz, 20 kHz at 192 kHz (near Nyquist).
    std::vector<Case> responseCases()
    {
        std::vector<Case> cases;

        for (auto fs : { 44100.0, 48000.0, 96000.0 })
            for (auto f0 : { 100.0, 1000.0, 5000.0 })
                cases.push_back ({ fs, f0 });

        for (auto f0 : { 10000.0, 20000.0 })
            cases.push_back ({ 192000.0, f0 });

        return cases;
    }

    const std::vector<double> gainsDb { -18.0, -12.0, -6.0, 6.0, 12.0, 18.0 };
    const std::vector<double> qs { 0.5, 1.0, 4.0 };
}

TEST_CASE ("Analog prototype has the expected shape", "[design]")
{
    CHECK_THAT (MatchedPeakingDesign::analogMagnitudeDb (1000.0, 1000.0, 12.0, 1.0), WithinAbs (12.0, 1e-9));
    CHECK_THAT (MatchedPeakingDesign::analogMagnitudeDb (1000.0, 1000.0, -9.0, 4.0), WithinAbs (-9.0, 1e-9));
    CHECK_THAT (MatchedPeakingDesign::analogMagnitudeDb (1.0e-3, 1000.0, 12.0, 1.0), WithinAbs (0.0, 1e-6));
    CHECK_THAT (MatchedPeakingDesign::analogMagnitudeDb (1.0e9, 1000.0, 12.0, 1.0), WithinAbs (0.0, 1e-6));
}

TEST_CASE ("Matched peaking design matches the analog prototype at f0", "[design]")
{
    for (const auto& c : responseCases())
    {
        for (auto gainDb : gainsDb)
        {
            for (auto q : qs)
            {
                INFO ("fs=" << c.sampleRate << " f0=" << c.centreHz << " gain=" << gainDb << " Q=" << q);
                const auto coeffs = MatchedPeakingDesign::design (c.centreHz, gainDb, q, c.sampleRate);
                CHECK_THAT (coeffs.magnitudeDb (c.centreHz, c.sampleRate), WithinAbs (gainDb, toleranceDb));
            }
        }
    }
}

TEST_CASE ("Matched peaking design stays within the stated bound of the analog prototype at +-1 octave", "[design]")
{
    for (const auto& c : responseCases())
    {
        for (auto gainDb : gainsDb)
        {
            for (auto q : qs)
            {
                const auto coeffs = MatchedPeakingDesign::design (c.centreHz, gainDb, q, c.sampleRate);

                for (auto f : { c.centreHz / 2.0, c.centreHz * 2.0 })
                {
                    if (f >= c.sampleRate / 2.0)
                        continue;

                    INFO ("fs=" << c.sampleRate << " f0=" << c.centreHz << " gain=" << gainDb
                                << " Q=" << q << " f=" << f);

                    const auto expected = MatchedPeakingDesign::analogMagnitudeDb (f, c.centreHz, gainDb, q);
                    CHECK_THAT (coeffs.magnitudeDb (f, c.sampleRate), WithinAbs (expected, analogOctaveBoundDb));
                }
            }
        }
    }
}

TEST_CASE ("Matched peaking design has its extremum at f0", "[design]")
{
    // Vicanek eq. 43, condition 3: d|H|^2/dw = 0 at w0. Checked as the slope in dB per
    // unit of ln f, by central difference.
    for (const auto& c : responseCases())
    {
        for (auto gainDb : gainsDb)
        {
            for (auto q : qs)
            {
                INFO ("fs=" << c.sampleRate << " f0=" << c.centreHz << " gain=" << gainDb << " Q=" << q);
                const auto coeffs = MatchedPeakingDesign::design (c.centreHz, gainDb, q, c.sampleRate);

                const auto h = c.centreHz * 1.0e-4;
                const auto slope = (coeffs.magnitudeDb (c.centreHz + h, c.sampleRate)
                                    - coeffs.magnitudeDb (c.centreHz - h, c.sampleRate)) / (2.0 * h) * c.centreHz;
                CHECK_THAT (slope, WithinAbs (0.0, 1.0e-3));
            }
        }
    }
}

TEST_CASE ("Matched peaking design has unity gain at DC", "[design]")
{
    for (const auto& c : responseCases())
    {
        for (auto gainDb : gainsDb)
        {
            INFO ("fs=" << c.sampleRate << " f0=" << c.centreHz << " gain=" << gainDb);
            const auto coeffs = MatchedPeakingDesign::design (c.centreHz, gainDb, 1.0, c.sampleRate);
            CHECK_THAT (coeffs.magnitudeDb (0.0, c.sampleRate), WithinAbs (0.0, 1e-6));
        }
    }
}

TEST_CASE ("Matched peaking design is stable across the parameter range", "[design]")
{
    for (auto fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (auto f0 : { 20.0, 100.0, 1000.0, 10000.0, 20000.0 })
        {
            for (auto gainDb : { -30.0, -12.0, 0.0, 12.0, 30.0 })
            {
                for (auto q : { 0.1, 0.71, 4.0, 18.0 })
                {
                    INFO ("fs=" << fs << " f0=" << f0 << " gain=" << gainDb << " Q=" << q);
                    const auto coeffs = MatchedPeakingDesign::design (f0, gainDb, q, fs);

                    CHECK (coeffs.isStable());
                    CHECK (std::isfinite (coeffs.b0));
                    CHECK (std::isfinite (coeffs.b1));
                    CHECK (std::isfinite (coeffs.b2));
                }
            }
        }
    }
}

TEST_CASE ("Matched peaking design at 0 dB is the identity", "[design]")
{
    for (auto fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (auto f0 : { 20.0, 1000.0, 20000.0 })
        {
            INFO ("fs=" << fs << " f0=" << f0);
            const auto coeffs = MatchedPeakingDesign::design (f0, 0.0, 1.0, fs);

            CHECK_THAT (coeffs.b0, WithinAbs (1.0, 1e-9));
            CHECK_THAT (coeffs.b1, WithinAbs (coeffs.a1, 1e-9));
            CHECK_THAT (coeffs.b2, WithinAbs (coeffs.a2, 1e-9));
        }
    }
}

TEST_CASE ("Matched peaking design stays within its stated bounds on the extended grid", "[design]")
{
    // Extended grid adds f0 = 10 and 16 kHz at 44.1 kHz (decision 2026-09-28).
    // Worst measured: octave 0.614 below 0.8 Nyquist, 2.640 at or above it.
    // The strict-grid bound (0.35 dB) is asserted by the test above.
    for (auto q : { 0.5, 1.0, 4.0 })
    {
        DesignTestGrid::checkAgainstAnalog (
            [q] (double f0, double gainDb, double fs) { return MatchedPeakingDesign::design (f0, gainDb, q, fs); },
            [q] (double f, double f0, double gainDb) { return MatchedPeakingDesign::analogMagnitudeDb (f, f0, gainDb, q); },
            DesignTestGrid::gainsDb, { 0.1, 0.65, 2.7, 0.1, 0.35 });
    }
}
