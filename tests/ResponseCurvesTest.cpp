#include "TestParameters.h"

#include "dsp/BandDesign.h"
#include "ui/ResponseCurves.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <numbers>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace TestParameters;

namespace
{
    std::array<BandSettings, 16> disabledBands()
    {
        std::array<BandSettings, 16> b;
        for (auto& s : b)
            s.enabled = false;
        return b;
    }

    BandSettings make (FilterType type, double f, double gain, double q, int slope = 3)
    {
        BandSettings s;
        s.type = type; s.frequencyHz = f; s.gainDb = gain; s.q = q; s.slopeIndex = slope;
        return s;
    }
}

TEST_CASE ("Curve points are log-spaced from 20 Hz to 20 kHz", "[curves]")
{
    ResponseCurves curves;
    CHECK_THAT (curves.frequency (0), WithinRel (20.0, 1e-12));
    CHECK_THAT (curves.frequency (ResponseCurves::numPoints - 1), WithinRel (20000.0, 1e-12));

    const auto ratio = curves.frequency (1) / curves.frequency (0);
    for (int k = 1; k < ResponseCurves::numPoints; ++k)
        CHECK_THAT (curves.frequency (k) / curves.frequency (k - 1), WithinRel (ratio, 1e-9));
}

TEST_CASE ("Band curves equal their designs and the sum equals the bands", "[curves]")
{
    constexpr double fs = 48000.0;
    auto bands = disabledBands();
    bands[0] = make (FilterType::lowCut, 40.0, 0.0, 0.71, 5);
    bands[3] = make (FilterType::bell, 400.0, 6.0, 2.0);
    bands[8] = make (FilterType::notch, 2200.0, 0.0, 4.0);
    bands[14] = make (FilterType::highShelf, 8000.0, -4.0, 0.71);

    ResponseCurves curves;
    REQUIRE (curves.update (bands, fs));

    for (int b = 0; b < 16; ++b)
    {
        INFO ("band index " << b);
        CHECK (curves.isBandActive (b) == bands[static_cast<size_t> (b)].enabled);
    }

    for (int k = 0; k < ResponseCurves::numPoints; k += 7)
    {
        const auto f = curves.frequency (k);
        double sum = 0.0;

        for (int b : { 0, 3, 8, 14 })
        {
            const auto expected = BandDesign::design (bands[static_cast<size_t> (b)], fs).magnitudeDb (f, fs);
            INFO ("band " << b << " f=" << f);
            CHECK_THAT (curves.bandDb (b, k), WithinAbs (expected, 1e-12));
            sum += expected;
        }

        CHECK_THAT (curves.sumDb (k), WithinAbs (sum, 1e-9));
    }
}

TEST_CASE ("With every band off the sum is flat", "[curves]")
{
    ResponseCurves curves;
    curves.update (disabledBands(), 48000.0);

    for (int k = 0; k < ResponseCurves::numPoints; ++k)
        CHECK_THAT (curves.sumDb (k), WithinAbs (0.0, 0.0));
}

TEST_CASE ("Curves recompute only when something changed", "[curves]")
{
    auto bands = disabledBands();
    bands[2] = make (FilterType::bell, 1000.0, 3.0, 1.0);

    ResponseCurves curves;
    CHECK (curves.update (bands, 48000.0));
    CHECK_FALSE (curves.update (bands, 48000.0));
    CHECK_FALSE (curves.update (bands, 48000.0));
    CHECK (curves.getNumRecomputes() == 1);

    bands[2].gainDb = 3.5;
    CHECK (curves.update (bands, 48000.0));
    CHECK (curves.update (bands, 96000.0));
    CHECK (curves.getNumRecomputes() == 3);
}

TEST_CASE ("Displayed sum matches the measured response of the plugin", "[curves]")
{
    // PLAN.md "Display accuracy": the plotted curve equals the measured response of the audio path.
    juce::ScopedJuceInitialiser_GUI juce;

    for (auto fs : { 44100.0, 48000.0, 96000.0 })
    {
        ParametricEQAudioProcessor p;
        setBand (p, 1, FilterType::lowCut, 40.0f, 0.0f, 0.71f, 5, true);
        setBand (p, 4, FilterType::bell, 400.0f, 6.0f, 2.0f, 3, true);
        setBand (p, 9, FilterType::notch, 2200.0f, 0.0f, 4.0f, 3, true);
        setBand (p, 15, FilterType::highShelf, 8000.0f, -4.0f, 0.71f, 3, true);
        setBand (p, 12, FilterType::flatTilt, 1000.0f, 5.0f, 0.71f, 3, true);

        p.setPlayConfigDetails (2, 2, fs, 512);
        p.prepareToPlay (fs, 512);

        constexpr int numSamples = 1 << 17;
        juce::AudioBuffer<float> ir (2, numSamples);
        ir.clear();
        ir.setSample (0, 0, 1.0f);
        juce::MidiBuffer midi;
        for (int start = 0; start < numSamples; start += 512)
        {
            juce::AudioBuffer<float> view (ir.getArrayOfWritePointers(), 2, start, 512);
            p.processBlock (view, midi);
        }

        const auto bands = p.getBandSettings();

        ResponseCurves curves;
        curves.update (bands, fs);

        for (int k = 0; k < ResponseCurves::numPoints; k += 16)
        {
            if (curves.sumDb (k) < -80.0)
                continue;

            const auto f = curves.frequency (k);
            const auto step = std::polar (1.0, -2.0 * std::numbers::pi * f / fs);
            std::complex<double> phasor { 1.0, 0.0 }, sum {};
            const auto* x = ir.getReadPointer (0);
            for (int n = 0; n < numSamples; ++n)
            {
                sum += static_cast<double> (x[n]) * phasor;
                phasor *= step;
                if ((n & 1023) == 1023)
                    phasor /= std::abs (phasor);
            }

            INFO ("fs=" << fs << " f=" << f);
            CHECK_THAT (20.0 * std::log10 (std::abs (sum)), WithinAbs (curves.sumDb (k), 0.1));
        }
    }
}

TEST_CASE ("Disabled bands are drawn but not summed; free bands are neither", "[curves][bandstate]")
{
    constexpr double fs = 48000.0;
    auto bands = disabledBands();
    for (auto& b : bands)
        b.inUse = false;

    bands[2] = make (FilterType::bell, 1000.0, 6.0, 1.0);           // in use, enabled
    bands[5] = make (FilterType::bell, 4000.0, -6.0, 1.0);          // in use, disabled
    bands[5].enabled = false;
    bands[8] = make (FilterType::bell, 200.0, 9.0, 1.0);            // enabled but free
    bands[8].inUse = false;

    ResponseCurves curves;
    curves.update (bands, fs);

    CHECK (curves.isBandShown (2));
    CHECK (curves.isBandActive (2));
    CHECK (curves.isBandShown (5));
    CHECK_FALSE (curves.isBandActive (5));
    CHECK_FALSE (curves.isBandShown (8));
    CHECK_FALSE (curves.isBandActive (8));

    auto asIfEnabled = bands[5];
    asIfEnabled.enabled = true;
    for (int k = 0; k < ResponseCurves::numPoints; k += 11)
    {
        const auto f = curves.frequency (k);
        CHECK_THAT (curves.bandDb (5, k), WithinAbs (BandDesign::design (asIfEnabled, fs).magnitudeDb (f, fs), 1e-12));
        CHECK_THAT (curves.sumDb (k), WithinAbs (BandDesign::design (bands[2], fs).magnitudeDb (f, fs), 1e-9));
    }
}

//==============================================================================
TEST_CASE ("A dynamic band's curve follows its live gain; its range curve spans static to static + range", "[curves][dynamics]")
{
    constexpr double fs = 48000.0;
    auto bands = disabledBands();
    bands[5] = make (FilterType::bell, 1000.0, 3.0, 1.0);
    bands[5].dynamics.on = true;
    bands[5].dynamics.rangeDb = -6.0;
    bands[7] = make (FilterType::bell, 5000.0, 2.0, 2.0);   // static

    std::array<double, 16> live {};
    live[5] = -4.0;
    live[7] = -3.0;   // ignored: band 8 is not dynamic

    ResponseCurves curves;
    REQUIRE (curves.update (bands, fs, live));

    // Nearest curve point to each centre.
    auto pointAt = [&] (double f)
    {
        int best = 0;
        for (int k = 0; k < ResponseCurves::numPoints; ++k)
            if (std::abs (std::log (curves.frequency (k) / f)) < std::abs (std::log (curves.frequency (best) / f)))
                best = k;
        return best;
    };
    const auto k1 = pointAt (1000.0), k5 = pointAt (5000.0);

    auto liveBand = bands[5];
    liveBand.gainDb = 3.0 - 4.0;
    auto rangeBand = bands[5];
    rangeBand.gainDb = 3.0 - 6.0;
    const auto f1 = curves.frequency (k1);

    CHECK (curves.isBandDynamic (5));
    CHECK_FALSE (curves.isBandDynamic (7));
    CHECK_THAT (curves.bandDb (5, k1), WithinAbs (BandDesign::design (liveBand, fs).magnitudeDb (f1, fs), 1e-9));
    CHECK_THAT (curves.staticBandDb (5, k1), WithinAbs (BandDesign::design (bands[5], fs).magnitudeDb (f1, fs), 1e-9));
    CHECK_THAT (curves.rangeBandDb (5, k1), WithinAbs (BandDesign::design (rangeBand, fs).magnitudeDb (f1, fs), 1e-9));
    CHECK_THAT (curves.bandDb (7, k5), WithinAbs (BandDesign::design (bands[7], fs).magnitudeDb (curves.frequency (k5), fs), 1e-9));

    // The sum uses the live curve.
    CHECK_THAT (curves.sumDb (k1), WithinAbs (curves.bandDb (5, k1) + curves.bandDb (7, k1), 1e-9));
}

TEST_CASE ("Live gain changes under 0.05 dB do not recompute the curves", "[curves][dynamics]")
{
    auto bands = disabledBands();
    bands[2] = make (FilterType::lowShelf, 200.0, 0.0, 0.71);
    bands[2].dynamics.on = true;

    std::array<double, 16> live {};
    ResponseCurves curves;
    CHECK (curves.update (bands, 48000.0, live));
    live[2] = -0.03;
    CHECK_FALSE (curves.update (bands, 48000.0, live));
    live[2] = -0.2;
    CHECK (curves.update (bands, 48000.0, live));
    live[6] = -5.0;   // a band that is not in use
    CHECK_FALSE (curves.update (bands, 48000.0, live));
}

TEST_CASE ("Disabled or non-dynamic-type bands ignore live gain", "[curves][dynamics]")
{
    constexpr double fs = 48000.0;
    auto bands = disabledBands();
    bands[0] = make (FilterType::bell, 500.0, 4.0, 1.0);
    bands[0].dynamics.on = true;
    bands[0].enabled = false;
    bands[1] = make (FilterType::notch, 3000.0, 0.0, 4.0);
    bands[1].dynamics.on = true;

    std::array<double, 16> live {};
    live[0] = -6.0;
    live[1] = -6.0;

    ResponseCurves curves;
    curves.update (bands, fs, live);
    CHECK_FALSE (curves.isBandDynamic (0));
    CHECK_FALSE (curves.isBandDynamic (1));

    auto enabled = bands[0];
    enabled.enabled = true;
    for (int k = 0; k < ResponseCurves::numPoints; k += 31)
    {
        const auto f = curves.frequency (k);
        CHECK_THAT (curves.bandDb (0, k), WithinAbs (BandDesign::design (enabled, fs).magnitudeDb (f, fs), 1e-9));
        CHECK_THAT (curves.bandDb (1, k), WithinAbs (BandDesign::design (bands[1], fs).magnitudeDb (f, fs), 1e-9));
    }
}
