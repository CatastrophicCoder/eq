#include "ui/AnalyzerSettings.h"
#include "ui/SpectrumAnalyzer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
    constexpr double fs = 48000.0;

    std::vector<float> sines (std::initializer_list<std::pair<double, double>> partials, int numSamples)
    {
        std::vector<float> x (static_cast<size_t> (numSamples));
        for (int n = 0; n < numSamples; ++n)
        {
            double v = 0.0;
            for (auto [f, a] : partials)
                v += a * std::sin (2.0 * std::numbers::pi * f * n / fs);
            x[static_cast<size_t> (n)] = static_cast<float> (v);
        }
        return x;
    }

    int nearestPoint (const SpectrumAnalyzer& a, double f)
    {
        int best = 0;
        for (int k = 1; k < SpectrumAnalyzer::numPoints; ++k)
            if (std::abs (std::log (a.frequency (k) / f)) < std::abs (std::log (a.frequency (best) / f)))
                best = k;
        return best;
    }

    /** Highest level within a third of an octave of f (covers bin/point rounding). */
    double levelNear (const SpectrumAnalyzer& a, double f)
    {
        double best = SpectrumAnalyzer::floorDb;
        for (int k = 0; k < SpectrumAnalyzer::numPoints; ++k)
            if (std::abs (std::log2 (a.frequency (k) / f)) <= 1.0 / 6.0)
                best = std::max (best, a.levelDb (k));
        return best;
    }

    /** A bin-centred frequency near f, so the sine sits on one bin. */
    double onBin (double f, int fftSize) { return std::round (f * fftSize / fs) * fs / fftSize; }
}

TEST_CASE ("Analyzer points are log-spaced from 20 Hz to 20 kHz and start at the floor", "[analyzer]")
{
    SpectrumAnalyzer a;
    CHECK_THAT (a.frequency (0), WithinRel (20.0, 1e-9));
    CHECK_THAT (a.frequency (SpectrumAnalyzer::numPoints - 1), WithinRel (20000.0, 1e-9));
    for (int k = 0; k < SpectrumAnalyzer::numPoints; ++k)
        CHECK_THAT (a.levelDb (k), WithinAbs (SpectrumAnalyzer::floorDb, 0.0));
}

TEST_CASE ("A full-scale sine reads 0 dB, and 20 dB lower reads -20 dB", "[analyzer]")
{
    for (auto order : AnalyzerSettings::fftOrders)
    {
        SpectrumAnalyzer a;
        a.setFftOrder (order);
        const auto f = onBin (1000.0, a.getFftSize());

        for (auto [amplitude, expected] : { std::pair { 1.0, 0.0 }, { 0.1, -20.0 } })
        {
            a.reset();
            const auto x = sines ({ { f, amplitude } }, a.getFftSize());
            a.addSamples (x.data(), static_cast<int> (x.size()));
            a.update (fs, 0.0);

            INFO ("fft " << a.getFftSize() << " amplitude " << amplitude);
            CHECK_THAT (levelNear (a, f), WithinAbs (expected, 0.5));
        }
    }
}

TEST_CASE ("A sine peaks at its own frequency", "[analyzer]")
{
    SpectrumAnalyzer a;
    a.setFftOrder (13);
    const auto f = onBin (5000.0, a.getFftSize());
    const auto x = sines ({ { f, 0.5 } }, a.getFftSize());
    a.addSamples (x.data(), static_cast<int> (x.size()));
    a.update (fs, 0.0);

    int loudest = 0;
    for (int k = 1; k < SpectrumAnalyzer::numPoints; ++k)
        if (a.levelDb (k) > a.levelDb (loudest))
            loudest = k;

    const auto binWidth = fs / a.getFftSize();
    CHECK (std::abs (a.frequency (loudest) - f) <= std::max (binWidth, a.frequency (loudest) * 0.02));
}

TEST_CASE ("Calibration is the same across the frequency range", "[analyzer]")
{
    // Equal-amplitude sines from 50 Hz to 15 kHz read equal levels. (A white-noise test would not
    // read flat: the log binning keeps the highest bin per point, which lifts wide high points.)
    SpectrumAnalyzer a;
    a.setFftOrder (14);
    std::vector<double> freqs;
    for (auto f : { 50.0, 200.0, 1000.0, 4000.0, 15000.0 })
        freqs.push_back (onBin (f, a.getFftSize()));

    const auto x = sines ({ { freqs[0], 0.1 }, { freqs[1], 0.1 }, { freqs[2], 0.1 }, { freqs[3], 0.1 }, { freqs[4], 0.1 } },
                          a.getFftSize());
    a.addSamples (x.data(), static_cast<int> (x.size()));
    a.update (fs, 0.0);

    for (auto f : freqs)
    {
        INFO ("f " << f);
        CHECK_THAT (levelNear (a, f), WithinAbs (-20.0, 0.5));
    }
}

TEST_CASE ("Levels rise at once and fall at the release rate", "[analyzer]")
{
    SpectrumAnalyzer a;
    a.setFftOrder (12);
    a.setReleaseDbPerSecond (25.0);
    const auto f = onBin (1000.0, a.getFftSize());
    const auto k = nearestPoint (a, f);

    const auto loud = sines ({ { f, 1.0 } }, a.getFftSize());
    a.addSamples (loud.data(), static_cast<int> (loud.size()));
    a.update (fs, 1.0 / 30.0);
    const auto start = a.levelDb (k);
    CHECK (start > -1.5);   // instant rise

    // Silence: after one second in 30 steps the level has fallen by the release rate.
    const std::vector<float> silence (static_cast<size_t> (a.getFftSize()), 0.0f);
    a.addSamples (silence.data(), static_cast<int> (silence.size()));
    for (int step = 0; step < 30; ++step)
        a.update (fs, 1.0 / 30.0);

    CHECK_THAT (a.levelDb (k), WithinAbs (start - 25.0, 1.0));
}

TEST_CASE ("Freeze holds the spectrum", "[analyzer]")
{
    SpectrumAnalyzer a;
    const auto f = onBin (2000.0, a.getFftSize());
    const auto k = nearestPoint (a, f);

    const auto x = sines ({ { f, 0.5 } }, a.getFftSize());
    a.addSamples (x.data(), static_cast<int> (x.size()));
    a.update (fs, 0.03);
    const auto held = a.levelDb (k);

    a.setFrozen (true);
    CHECK (a.isFrozen());
    const std::vector<float> silence (static_cast<size_t> (a.getFftSize()), 0.0f);
    a.addSamples (silence.data(), static_cast<int> (silence.size()));
    for (int step = 0; step < 30; ++step)
        a.update (fs, 0.1);
    CHECK_THAT (a.levelDb (k), WithinAbs (held, 0.0));

    a.setFrozen (false);
    a.update (fs, 1.0);
    CHECK (a.levelDb (k) < held - 10.0);
}

TEST_CASE ("The display tilt is 4.5 dB per octave around 1 kHz", "[analyzer]")
{
    SpectrumAnalyzer a;
    const auto x = sines ({ { 1000.0, 0.5 } }, a.getFftSize());
    a.addSamples (x.data(), static_cast<int> (x.size()));
    a.update (fs, 0.0);

    for (int k = 0; k < SpectrumAnalyzer::numPoints; k += 37)
        CHECK_THAT (a.displayDb (k), WithinAbs (a.levelDb (k) + 4.5 * std::log2 (a.frequency (k) / 1000.0), 1e-9));
}

TEST_CASE ("Changing resolution and resetting", "[analyzer]")
{
    SpectrumAnalyzer a;
    for (auto order : AnalyzerSettings::fftOrders)
    {
        a.setFftOrder (order);
        CHECK (a.getFftSize() == (1 << order));
    }

    const auto x = sines ({ { 1000.0, 0.5 } }, a.getFftSize());
    a.addSamples (x.data(), static_cast<int> (x.size()));
    a.update (fs, 0.0);
    a.reset();
    for (int k = 0; k < SpectrumAnalyzer::numPoints; ++k)
        CHECK_THAT (a.levelDb (k), WithinAbs (SpectrumAnalyzer::floorDb, 0.0));
}

#include "ui/ResponseDisplay.h"

TEST_CASE ("Pre and post analyzer colours are clearly different", "[analyzer][colour]")
{
    // Owner feedback after M6: both were grey and hard to tell apart.
    const auto pre = ResponseDisplay::preAnalyzerColour();
    const auto post = ResponseDisplay::postAnalyzerColour();

    const auto distance = std::hypot (pre.getRed() - post.getRed(), pre.getGreen() - post.getGreen(), pre.getBlue() - post.getBlue());
    INFO ("RGB distance " << distance);
    CHECK (distance > 80.0);

    // Pre is a (muted) blue, post a warm light grey.
    CHECK (pre.getBlue() > pre.getRed() + 40);
    CHECK (post.getRed() >= post.getBlue());
    CHECK (post.getBrightness() > pre.getBrightness());
}
