#include "DesignTestGrid.h"

#include "dsp/FlatTiltDesign.h"
#include "dsp/MatchedOnePoleShelfDesign.h"
#include "dsp/MatchedShelfDesign.h"
#include "dsp/TiltShelfDesign.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using namespace DesignTestGrid;

TEST_CASE ("Shelves stay within their stated bounds of the analog prototype", "[design][shelf]")
{
    // Worst measured (2026-09-28): centre 0.123 (extended), 0.004 (strict);
    // octave 0.080 (extended), 0.012 (strict); near Nyquist 0.248.
    const Bounds bounds { 0.15, 0.1, 0.25, 0.1, 0.1 };

    SECTION ("low shelf")
    {
        checkAgainstAnalog (MatchedShelfDesign::designLow, MatchedShelfDesign::analogLowMagnitudeDb, gainsDb, bounds);
    }

    SECTION ("high shelf")
    {
        checkAgainstAnalog (MatchedShelfDesign::designHigh, MatchedShelfDesign::analogHighMagnitudeDb, gainsDb, bounds);
    }
}

TEST_CASE ("Shelves meet their matching conditions at DC and Nyquist", "[design][shelf]")
{
    // Vicanek 2024/25: unity DC (high shelf), and the analog magnitude at Nyquist (eq. 9-10).
    for (const auto& c : extendedCases())
    {
        for (auto gainDb : gainsDb)
        {
            INFO ("fs=" << c.sampleRate << " fc=" << c.centreHz << " gain=" << gainDb);
            const auto nyquist = c.sampleRate / 2.0;
            const auto high = MatchedShelfDesign::designHigh (c.centreHz, gainDb, c.sampleRate);
            const auto low  = MatchedShelfDesign::designLow  (c.centreHz, gainDb, c.sampleRate);

            CHECK_THAT (high.magnitudeDb (0.0, c.sampleRate), WithinAbs (0.0, 1e-9));
            CHECK_THAT (low.magnitudeDb (0.0, c.sampleRate), WithinAbs (gainDb, 1e-9));
            CHECK_THAT (high.magnitudeDb (nyquist, c.sampleRate),
                        WithinAbs (MatchedShelfDesign::analogHighMagnitudeDb (nyquist, c.centreHz, gainDb), 1e-6));
            CHECK_THAT (low.magnitudeDb (nyquist, c.sampleRate),
                        WithinAbs (MatchedShelfDesign::analogLowMagnitudeDb (nyquist, c.centreHz, gainDb), 1e-6));
        }
    }
}

TEST_CASE ("Shelves at 0 dB are flat", "[design][shelf]")
{
    // The paper substitutes G = 1.00001 at unity gain (appendix A), i.e. at most ~1e-4 dB.
    for (auto fs : { 44100.0, 96000.0 })
    {
        const auto high = MatchedShelfDesign::designHigh (1000.0, 0.0, fs);
        const auto low  = MatchedShelfDesign::designLow  (1000.0, 0.0, fs);

        for (auto f : { 20.0, 1000.0, 15000.0 })
        {
            CHECK_THAT (high.magnitudeDb (f, fs), WithinAbs (0.0, 1e-3));
            CHECK_THAT (low.magnitudeDb (f, fs), WithinAbs (0.0, 1e-3));
        }
    }
}

TEST_CASE ("One-pole shelf meets its matching conditions", "[design][shelf]")
{
    // Vicanek 2019: unity at DC (eq. 6) and the analog value at f_m = 0.9 Nyquist (eq. 12).
    for (const auto& c : extendedCases())
    {
        for (auto gainDb : gainsDb)
        {
            INFO ("fs=" << c.sampleRate << " fc=" << c.centreHz << " gain=" << gainDb);
            const auto shelf = MatchedOnePoleShelfDesign::designHigh (c.centreHz, gainDb, c.sampleRate);
            const auto fm = 0.9 * c.sampleRate / 2.0;

            CHECK_THAT (shelf.magnitudeDb (0.0, c.sampleRate), WithinAbs (0.0, 1e-9));
            CHECK_THAT (shelf.magnitudeDb (fm, c.sampleRate),
                        WithinAbs (MatchedOnePoleShelfDesign::analogHighMagnitudeDb (fm, c.centreHz, gainDb), 1e-6));
        }
    }
}

TEST_CASE ("Tilt shelf stays within its stated bounds of the analog prototype", "[design][tilt]")
{
    // Worst measured (2026-09-28), f_m = 0.9: centre 0.385 (extended), 0.079 (strict);
    // octave 0.222 (both grids); near Nyquist 0.024.
    checkAgainstAnalog (TiltShelfDesign::design, TiltShelfDesign::analogMagnitudeDb, gainsDb,
                        { 0.4, 0.25, 0.05, 0.1, 0.25 });
}

TEST_CASE ("Tilt shelf sits at -gain/2 at DC", "[design][tilt]")
{
    for (auto gainDb : gainsDb)
        CHECK_THAT (TiltShelfDesign::design (1000.0, gainDb, 48000.0).magnitudeDb (0.0, 48000.0),
                    WithinAbs (-gainDb / 2.0, 1e-9));
}

TEST_CASE ("Flat tilt follows a straight line within its stated bound", "[design][flattilt]")
{
    // Prototype (2026-09-28): 0.223 dB worst, 20 Hz - 0.9 Nyquist, slopes up to 3 dB/oct. Bound 0.25 dB.
    constexpr double bound = 0.25;

    for (auto fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (auto pivot : { 100.0, 1000.0, 5000.0 })
        {
            for (auto gainDb : { -30.0, -10.0, 10.0, 30.0 })
            {
                const auto tilt = FlatTiltDesign::design (pivot, gainDb, fs);

                CHECK (tilt.numSections == FlatTiltDesign::numShelves);
                CHECK_THAT (tilt.magnitudeDb (pivot, fs), WithinAbs (0.0, 1e-9));

                for (int k = 0; k < 200; ++k)
                {
                    const auto f = 20.0 * std::pow (1000.0, k / 199.0);
                    if (f > 0.9 * fs / 2.0)
                        continue;

                    INFO ("fs=" << fs << " pivot=" << pivot << " gain=" << gainDb << " f=" << f);
                    CHECK (std::abs (tilt.magnitudeDb (f, fs) - FlatTiltDesign::idealMagnitudeDb (f, pivot, gainDb)) <= bound);
                }
            }
        }
    }
}

TEST_CASE ("Shelf, tilt and flat tilt designs are stable across the parameter range", "[design]")
{
    for (auto fs : stabilitySampleRates)
    {
        for (auto f0 : stabilityFrequencies)
        {
            for (auto gainDb : stabilityGains)
            {
                INFO ("fs=" << fs << " f0=" << f0 << " gain=" << gainDb);
                CHECK (isFiniteAndStable (MatchedShelfDesign::designLow (f0, gainDb, fs)));
                CHECK (isFiniteAndStable (MatchedShelfDesign::designHigh (f0, gainDb, fs)));
                CHECK (isFiniteAndStable (TiltShelfDesign::design (f0, gainDb, fs)));

                const auto flat = FlatTiltDesign::design (f0, gainDb, fs);
                CHECK (flat.numSections == FlatTiltDesign::numShelves);
                CHECK (flat.isStable());
            }
        }
    }
}
