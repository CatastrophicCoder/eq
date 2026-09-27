#include "dsp/MatchedPeakingDesign.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double toleranceDb = 0.1;

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

TEST_CASE ("Matched peaking design matches the analog prototype at f0 and +-1 octave", "[design]")
{
    for (const auto& c : responseCases())
    {
        for (auto gainDb : gainsDb)
        {
            for (auto q : qs)
            {
                const auto coeffs = MatchedPeakingDesign::design (c.centreHz, gainDb, q, c.sampleRate);

                for (auto f : { c.centreHz / 2.0, c.centreHz, c.centreHz * 2.0 })
                {
                    if (f >= c.sampleRate / 2.0)
                        continue;

                    INFO ("fs=" << c.sampleRate << " f0=" << c.centreHz << " gain=" << gainDb
                                << " Q=" << q << " f=" << f);

                    const auto expected = MatchedPeakingDesign::analogMagnitudeDb (f, c.centreHz, gainDb, q);
                    CHECK_THAT (coeffs.magnitudeDb (f, c.sampleRate), WithinAbs (expected, toleranceDb));
                }
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
