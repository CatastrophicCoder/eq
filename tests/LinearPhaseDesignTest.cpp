#include "dsp/BandDesign.h"
#include "dsp/CutSlope.h"
#include "dsp/LinearPhaseDesigner.h"
#include "dsp/StereoTransfer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <numbers>

using Catch::Matchers::WithinAbs;

namespace
{
    std::array<BandSettings, 16> freeBands()
    {
        std::array<BandSettings, 16> b;
        for (auto& s : b)
            s.inUse = false;
        return b;
    }

    BandSettings make (FilterType type, double f, double gain, double q, int slope = 3)
    {
        BandSettings s;
        s.type = type; s.frequencyHz = f; s.gainDb = gain; s.q = q; s.slopeIndex = slope;
        return s;
    }

    /** Magnitude of an FIR at one frequency (direct DTFT, double). The linear phase is removed by
        taking the magnitude; the sign of a real zero-phase response is kept via the centre tap's frame. */
    double firResponse (const std::vector<float>& taps, double f, double fs)
    {
        const auto centre = static_cast<double> (taps.size()) / 2.0;
        double re = 0.0;
        for (size_t n = 0; n < taps.size(); ++n)
            re += taps[n] * std::cos (2.0 * std::numbers::pi * f / fs * (static_cast<double> (n) - centre));
        return re;   // symmetric about the centre: the zero-phase response is real
    }

    double toDb (double v) { return 20.0 * std::log10 (std::max (1.0e-12, std::abs (v))); }

    /** Log-spaced test points from fMin to min (20 kHz, 0.45 fs). */
    std::vector<double> testPoints (double fMin, double fs, int count = 160)
    {
        const auto fMax = std::min (20000.0, 0.45 * fs);
        std::vector<double> f;
        for (int i = 0; i < count; ++i)
            f.push_back (fMin * std::pow (fMax / fMin, i / (count - 1.0)));
        return f;
    }

    struct Shape { const char* name; BandSettings band; bool isCut; };

    std::vector<Shape> shapes()
    {
        return { { "bell 1 kHz +6 Q1", make (FilterType::bell, 1000.0, 6.0, 1.0), false },
                 { "bell 100 Hz -9 Q4", make (FilterType::bell, 100.0, -9.0, 4.0), false },
                 { "bell 60 Hz +6 Q0.7", make (FilterType::bell, 60.0, 6.0, 0.7), false },
                 { "bell 8 kHz +4 Q2", make (FilterType::bell, 8000.0, 4.0, 2.0), false },
                 { "bell 3 kHz -12 Q10", make (FilterType::bell, 3000.0, -12.0, 10.0), false },
                 { "low shelf 120 Hz +6", make (FilterType::lowShelf, 120.0, 6.0, 0.71), false },
                 { "high shelf 6 kHz -6", make (FilterType::highShelf, 6000.0, -6.0, 0.71), false },
                 { "notch 1 kHz Q8", make (FilterType::notch, 1000.0, 0.0, 8.0), false },
                 { "band pass 2 kHz Q2", make (FilterType::bandPass, 2000.0, 0.0, 2.0), false },
                 { "tilt shelf 1 kHz +4", make (FilterType::tiltShelf, 1000.0, 4.0, 0.71), false },
                 { "flat tilt +6", make (FilterType::flatTilt, 1000.0, 6.0, 0.71), false },
                 { "low cut 80 Hz 24 dB/oct", make (FilterType::lowCut, 80.0, 0.0, 0.71, 3), true },
                 { "high cut 12 kHz 48 dB/oct", make (FilterType::highCut, 12000.0, 0.0, 0.71, 7), true },
                 { "low cut 300 Hz Brickwall", make (FilterType::lowCut, 300.0, 0.0, 0.71, CutSlope::brickwallIndex), true } };
    }

    /** Stated bounds (dB) above the lowest accurate frequency; see docs/PROGRESS.md. */
    constexpr double smoothBoundDb = 0.1;    // bells, shelves, tilts, band pass, notch
    constexpr double cutBoundDb = 0.5;       // cut filters, outside their transition band
    constexpr double floorDb = -80.0;        // both below: pass

    /** Lowest accurate frequency: a fixed number of FFT bins of the filter length. */
    double lowestAccurateHz (int numTaps, double fs) { return LinearPhaseDesigner::lowestAccurateBins * fs / numTaps; }
}

//==============================================================================
TEST_CASE ("Linear-phase taps: three lengths, latency half the length", "[linearphase][design]")
{
    CHECK (LinearPhaseDesigner::tapCounts == std::array<int, 3> { 8192, 16384, 32768 });
    for (auto n : LinearPhaseDesigner::tapCounts)
        CHECK (LinearPhaseDesigner::latencyFor (n) == n / 2);
}

TEST_CASE ("A flat EQ designs a pure delay of half the length", "[linearphase][design]")
{
    LinearPhaseDesigner designer;
    LinearPhaseDesigner::Result r;

    auto bands = freeBands();
    bands[3] = make (FilterType::bell, 1000.0, 6.0, 1.0);
    bands[3].enabled = false;   // disabled bands are left out
    designer.design (bands, 48000.0, 8192, r);

    REQUIRE (r.numTaps == 8192);
    CHECK_FALSE (r.hasCrossTerms);
    for (auto* taps : { &r.leftFromLeft, &r.rightFromRight })
    {
        REQUIRE (taps->size() == 8192u);
        for (size_t n = 0; n < taps->size(); ++n)
            REQUIRE_THAT ((*taps)[n], WithinAbs (n == 4096 ? 1.0f : 0.0f, 1.0e-6f));
    }
}

TEST_CASE ("Linear-phase taps are symmetric about the centre", "[linearphase][design]")
{
    LinearPhaseDesigner designer;
    LinearPhaseDesigner::Result r;
    auto bands = freeBands();
    bands[0] = make (FilterType::lowCut, 60.0, 0.0, 0.71, 5);
    bands[4] = make (FilterType::bell, 900.0, -5.0, 2.0);
    bands[9] = make (FilterType::highShelf, 7000.0, 3.0, 0.71);
    bands[9].channel = ChannelMode::side;

    for (auto n : LinearPhaseDesigner::tapCounts)
    {
        designer.design (bands, 48000.0, n, r);
        INFO ("taps " << n);
        REQUIRE (r.hasCrossTerms);
        for (auto* taps : { &r.leftFromLeft, &r.leftFromRight, &r.rightFromLeft, &r.rightFromRight })
        {
            REQUIRE (taps->size() == static_cast<size_t> (n));
            CHECK (juce::exactlyEqual ((*taps)[0], 0.0f));   // the unpaired first tap is zero: odd effective length n - 1
            for (int j = 1; j < n / 2; ++j)
                REQUIRE (juce::exactlyEqual ((*taps)[static_cast<size_t> (n / 2 + j)], (*taps)[static_cast<size_t> (n / 2 - j)]));
        }
    }
}

TEST_CASE ("Linear-phase magnitude matches the curve above the lowest accurate frequency", "[linearphase][design]")
{
    LinearPhaseDesigner designer;
    LinearPhaseDesigner::Result r;

    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (auto n : LinearPhaseDesigner::tapCounts)
        {
            const auto fMin = lowestAccurateHz (n, fs);

            for (const auto& shape : shapes())
            {
                auto bands = freeBands();
                bands[5] = shape.band;
                designer.design (bands, fs, n, r);
                const auto design = BandDesign::design (shape.band, fs);

                double worst = 0.0, worstAt = 0.0;
                for (auto f : testPoints (fMin, fs))
                {
                    const auto target = design.magnitudeDb (f, fs);
                    const auto measured = toDb (firResponse (r.leftFromLeft, f, fs));
                    if (target < floorDb && measured < floorDb)
                        continue;

                    // Cuts: skip their transition band (within a factor of 2^(24/slope dB) of the cutoff),
                    // where the window's smoothing dominates; the slope rule is checked separately.
                    if (shape.isCut)
                    {
                        const auto octaves = std::abs (std::log2 (f / shape.band.frequencyHz));
                        if (octaves < std::max (0.25, 24.0 / CutSlope::dbPerOctave (shape.band.slopeIndex)))
                            continue;
                    }

                    if (std::abs (measured - target) > worst)
                    {
                        worst = std::abs (measured - target);
                        worstAt = f;
                    }
                }

                INFO (shape.name << ", " << n << " taps at " << fs << " Hz (lowest accurate " << fMin << " Hz): worst "
                                 << worst << " dB at " << worstAt << " Hz");
                CHECK (worst <= (shape.isCut ? cutBoundDb : smoothBoundDb));
            }
        }
}

TEST_CASE ("Linear-phase cut slopes: one and two octaves past the cutoff match the IIR design", "[linearphase][design]")
{
    // A 24 dB/oct low cut at 400 Hz, 32768 taps at 48 kHz: the FIR's attenuation at 200 and 100 Hz
    // equals the IIR design's (which the cut tests tie to 6 dB/oct per order) within the cut bound.
    LinearPhaseDesigner designer;
    LinearPhaseDesigner::Result r;
    auto bands = freeBands();
    bands[0] = make (FilterType::lowCut, 400.0, 0.0, 0.71, 3);
    designer.design (bands, 48000.0, 32768, r);
    const auto design = BandDesign::design (bands[0], 48000.0);

    for (double f : { 200.0, 100.0 })
        CHECK_THAT (toDb (firResponse (r.leftFromLeft, f, 48000.0)), WithinAbs (design.magnitudeDb (f, 48000.0), 0.5));
}

TEST_CASE ("Linear-phase stereo matrix: Left, Right, Mid and Side bands land in the right terms", "[linearphase][design]")
{
    constexpr double fs = 48000.0;
    LinearPhaseDesigner designer;
    LinearPhaseDesigner::Result r;
    const auto bell = make (FilterType::bell, 2000.0, 8.0, 1.0);
    const auto fMin = lowestAccurateHz (16384, fs);
    const auto h = [&] (double f) { return std::pow (10.0, BandDesign::design (bell, fs).magnitudeDb (f, fs) / 20.0); };

    for (auto mode : { ChannelMode::stereo, ChannelMode::left, ChannelMode::right, ChannelMode::mid, ChannelMode::side })
    {
        auto bands = freeBands();
        bands[2] = bell;
        bands[2].channel = mode;
        designer.design (bands, fs, 16384, r);

        INFO ("mode " << ChannelModes::names[static_cast<int> (mode)]);
        CHECK (r.hasCrossTerms == ChannelModes::isMidSide (mode));

        for (auto f : testPoints (fMin, fs, 24))
        {
            const auto m = StereoTransfer::forBand (mode, { h (f), 0.0 });
            CHECK_THAT (firResponse (r.leftFromLeft, f, fs), WithinAbs (m[0][0].real(), 0.005));
            CHECK_THAT (firResponse (r.rightFromRight, f, fs), WithinAbs (m[1][1].real(), 0.005));
            if (r.hasCrossTerms)
            {
                CHECK_THAT (firResponse (r.leftFromRight, f, fs), WithinAbs (m[0][1].real(), 0.005));
                CHECK_THAT (firResponse (r.rightFromLeft, f, fs), WithinAbs (m[1][0].real(), 0.005));
            }
        }
    }
}

TEST_CASE ("Dynamic bands are left out of the linear-phase filter", "[linearphase][design][dynamics]")
{
    // Decision 2026-09-29: dynamic bands run as normal filters after the FIR.
    LinearPhaseDesigner designer;
    LinearPhaseDesigner::Result r;
    auto bands = freeBands();
    bands[7] = make (FilterType::bell, 1000.0, 6.0, 1.0);
    bands[7].dynamics.on = true;
    designer.design (bands, 48000.0, 8192, r);

    for (size_t n = 0; n < r.leftFromLeft.size(); ++n)
        REQUIRE_THAT (r.leftFromLeft[n], WithinAbs (n == 4096 ? 1.0f : 0.0f, 1.0e-6f));

    // A notch cannot be dynamic: dynamics on or off, it is in the filter.
    bands[7] = make (FilterType::notch, 1000.0, 0.0, 4.0);
    bands[7].dynamics.on = true;
    designer.design (bands, 48000.0, 8192, r);
    CHECK (toDb (firResponse (r.leftFromLeft, 1000.0, 48000.0)) < -20.0);
}
