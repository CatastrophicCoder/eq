#include "DesignTestGrid.h"

#include "dsp/ButterworthCascade.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

using Catch::Matchers::WithinAbs;
using namespace DesignTestGrid;
using Kind = ButterworthCascade::Kind;

namespace
{
    /** Orders 1-16 (6-96 dB/oct) and 32 (Brickwall). */
    std::vector<int> orders()
    {
        std::vector<int> result;
        for (int n = 1; n <= 16; ++n)
            result.push_back (n);
        result.push_back (32);
        return result;
    }

    /** Below this, digital and analog are both treated as "fully attenuated" (decision 2026-09-28). */
    constexpr double floorDb = -120.0;
}

TEST_CASE ("Butterworth section Qs match the standard tables", "[design][cut]")
{
    CHECK_THAT (ButterworthCascade::sectionQ (2, 1), WithinAbs (0.7071, 1e-4));
    CHECK_THAT (ButterworthCascade::sectionQ (3, 1), WithinAbs (1.0,    1e-4));
    CHECK_THAT (ButterworthCascade::sectionQ (4, 1), WithinAbs (0.5412, 1e-4));
    CHECK_THAT (ButterworthCascade::sectionQ (4, 2), WithinAbs (1.3066, 1e-4));
    CHECK_THAT (ButterworthCascade::sectionQ (5, 1), WithinAbs (0.6180, 1e-4));
    CHECK_THAT (ButterworthCascade::sectionQ (5, 2), WithinAbs (1.6180, 1e-4));
}

TEST_CASE ("Cut cascades use ceil(order / 2) sections", "[design][cut]")
{
    for (auto n : orders())
    {
        INFO ("order=" << n);
        CHECK (ButterworthCascade::design (Kind::lowCut, 1000.0, n, 48000.0).numSections == (n + 1) / 2);
        CHECK (ButterworthCascade::design (Kind::highCut, 1000.0, n, 48000.0).numSections == (n + 1) / 2);
    }
}

TEST_CASE ("Cut cascades are -3 dB at the cutoff", "[design][cut]")
{
    for (auto kind : { Kind::lowCut, Kind::highCut })
    {
        for (const auto& c : extendedCases())
        {
            for (auto n : orders())
            {
                INFO ((kind == Kind::lowCut ? "low cut" : "high cut") << " fs=" << c.sampleRate
                      << " fc=" << c.centreHz << " order=" << n);
                const auto cascade = ButterworthCascade::design (kind, c.centreHz, n, c.sampleRate);
                CHECK_THAT (cascade.magnitudeDb (c.centreHz, c.sampleRate),
                            WithinAbs (ButterworthCascade::analogMagnitudeDb (kind, c.centreHz, c.centreHz, n), 0.1));
            }
        }
    }
}

TEST_CASE ("Cut slopes one and two octaves past the cutoff follow analog Butterworth", "[design][cut]")
{
    // Analog Butterworth falls ~6 dB/oct per order past the cutoff, so matching it checks the slope.
    // Worst measured (2026-09-28), orders 1-16 and 32, -120 dB floor:
    //   low cut:  0.454 below 0.8 Nyquist (0.004 on the strict grid)
    //   high cut: 0.277 below 0.8 Nyquist (both grids), 12.218 at or above 0.8 Nyquist
    struct CutBounds { Kind kind; const char* name; double octave, nearNyquist, strict; };

    for (const auto& b : { CutBounds { Kind::lowCut,  "low cut",  0.5, 0.5,   0.1 },
                           CutBounds { Kind::highCut, "high cut", 0.3, 12.25, 0.3 } })
    {
        for (const auto& c : extendedCases())
        {
            for (auto n : orders())
            {
                const auto cascade = ButterworthCascade::design (b.kind, c.centreHz, n, c.sampleRate);

                for (auto octaves : { 1, 2 })
                {
                    const auto f = b.kind == Kind::highCut ? c.centreHz * (1 << octaves) : c.centreHz / (1 << octaves);
                    if (f >= c.sampleRate / 2.0)
                        continue;

                    const auto digital = cascade.magnitudeDb (f, c.sampleRate);
                    const auto analog = ButterworthCascade::analogMagnitudeDb (b.kind, f, c.centreHz, n);

                    if (digital < floorDb && analog < floorDb)
                        continue;

                    INFO (b.name << " fs=" << c.sampleRate << " fc=" << c.centreHz << " order=" << n
                                 << " +" << octaves << " oct: digital=" << digital << " analog=" << analog);
                    const auto error = std::abs (digital - analog);

                    CHECK (error <= (isNearNyquist (f, c.sampleRate) ? b.nearNyquist : b.octave));

                    if (c.inStrictGrid && ! isNearNyquist (f, c.sampleRate))
                        CHECK (error <= b.strict);
                }
            }
        }
    }
}

TEST_CASE ("Cut cascades are stable across the parameter range", "[design][cut]")
{
    for (auto kind : { Kind::lowCut, Kind::highCut })
    {
        for (auto fs : stabilitySampleRates)
        {
            for (auto fc : stabilityFrequencies)
            {
                for (auto n : orders())
                {
                    INFO ("fs=" << fs << " fc=" << fc << " order=" << n);
                    const auto cascade = ButterworthCascade::design (kind, fc, n, fs);
                    CHECK (cascade.numSections == (n + 1) / 2);

                    for (int i = 0; i < cascade.numSections; ++i)
                        CHECK (isFiniteAndStable (cascade.sections[static_cast<size_t> (i)]));
                }
            }
        }
    }
}
