#include "DesignTestGrid.h"

#include "dsp/MatchedAllpassDesign.h"
#include "dsp/MatchedBandpassDesign.h"
#include "dsp/MatchedNotchDesign.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using namespace DesignTestGrid;

TEST_CASE ("Band pass stays within its stated bounds of the analog prototype", "[design][bandpass]")
{
    // Worst measured (2026-09-28): centre 0.000; octave 0.645 (extended), 0.049 (strict);
    // near Nyquist 1.378.
    checkAgainstAnalog (MatchedBandpassDesign::design, MatchedBandpassDesign::analogMagnitudeDb, qs,
                        { 0.1, 0.7, 1.4, 0.1, 0.1 });
}

TEST_CASE ("Band pass has a zero at DC", "[design][bandpass]")
{
    const auto c = MatchedBandpassDesign::design (1000.0, 1.0, 48000.0);
    CHECK_THAT (c.b0 + c.b1 + c.b2, WithinAbs (0.0, 1e-12));
}

TEST_CASE ("Notch stays within its stated bounds of the analog prototype", "[design][notch]")
{
    // Worst measured (2026-09-28): octave 0.538 (extended), 0.081 (strict); near Nyquist 1.754.
    // The centre is checked as notch depth below instead (analog is -inf there).
    checkAgainstAnalog (MatchedNotchDesign::design, MatchedNotchDesign::analogMagnitudeDb, qs,
                        { 0.0, 0.55, 1.8, 0.0, 0.1 }, false);
}

TEST_CASE ("Notch is deep at f0 and unity at DC", "[design][notch]")
{
    for (const auto& c : extendedCases())
    {
        for (auto q : qs)
        {
            INFO ("fs=" << c.sampleRate << " f0=" << c.centreHz << " Q=" << q);
            const auto coeffs = MatchedNotchDesign::design (c.centreHz, q, c.sampleRate);

            CHECK (coeffs.magnitudeDb (c.centreHz, c.sampleRate) < -100.0);
            CHECK_THAT (coeffs.magnitudeDb (0.0, c.sampleRate), WithinAbs (0.0, 1e-9));
        }
    }
}

TEST_CASE ("All pass has unity magnitude everywhere", "[design][allpass]")
{
    for (const auto& c : extendedCases())
    {
        for (auto q : qs)
        {
            const auto coeffs = MatchedAllpassDesign::design (c.centreHz, q, c.sampleRate);

            for (int k = 1; k < 200; ++k)
            {
                const auto f = c.sampleRate / 2.0 * k / 200.0;
                INFO ("fs=" << c.sampleRate << " f0=" << c.centreHz << " Q=" << q << " f=" << f);
                CHECK_THAT (coeffs.magnitudeDb (f, c.sampleRate), WithinAbs (0.0, 1e-9));
            }
        }
    }
}

TEST_CASE ("Band pass, notch and all pass are stable across the parameter range", "[design]")
{
    for (auto fs : stabilitySampleRates)
    {
        for (auto f0 : stabilityFrequencies)
        {
            for (auto q : stabilityQs)
            {
                INFO ("fs=" << fs << " f0=" << f0 << " Q=" << q);
                CHECK (isFiniteAndStable (MatchedBandpassDesign::design (f0, q, fs)));
                CHECK (isFiniteAndStable (MatchedNotchDesign::design (f0, q, fs)));
                CHECK (isFiniteAndStable (MatchedAllpassDesign::design (f0, q, fs)));
            }
        }
    }
}
