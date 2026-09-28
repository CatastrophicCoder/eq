#include "ui/LevelMeter.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double fs = 48000.0;

    std::vector<float> sine (double amplitude, int numSamples)
    {
        std::vector<float> x (static_cast<size_t> (numSamples));
        for (int n = 0; n < numSamples; ++n)
            x[static_cast<size_t> (n)] = static_cast<float> (amplitude * std::sin (2.0 * std::numbers::pi * 997.0 * n / fs));
        return x;
    }
}

TEST_CASE ("A 0.5 sine reads -6 dB peak and -9 dB RMS", "[meter]")
{
    LevelMeter m;
    const auto x = sine (0.5, static_cast<int> (fs));   // 1 s: RMS settled (3.3 time constants)
    m.addSamples (x.data(), x.data(), static_cast<int> (x.size()), fs);
    m.update (0.0);

    for (int ch = 0; ch < 2; ++ch)
    {
        CHECK_THAT (m.peakDb (ch), WithinAbs (-6.02, 0.05));
        CHECK_THAT (m.rmsDb (ch), WithinAbs (-9.03, 0.2));
        CHECK_FALSE (m.isClipped (ch));
    }
}

TEST_CASE ("Peak holds for a second, then falls at 20 dB/s", "[meter]")
{
    LevelMeter m;
    const auto x = sine (0.5, 4800);
    m.addSamples (x.data(), x.data(), 4800, fs);
    m.update (0.0);
    const auto start = m.peakDb (0);

    const std::vector<float> silence (4800, 0.0f);
    for (int step = 0; step < 9; ++step)            // 0.9 s: still held
    {
        m.addSamples (silence.data(), silence.data(), 4800, fs);
        m.update (0.1);
    }
    CHECK_THAT (m.peakDb (0), WithinAbs (start, 1e-9));

    for (int step = 0; step < 11; ++step)           // to 2.0 s: 1 s of fall
    {
        m.addSamples (silence.data(), silence.data(), 4800, fs);
        m.update (0.1);
    }
    CHECK_THAT (m.peakDb (0), WithinAbs (start - 20.0, 0.5));
}

TEST_CASE ("RMS decays with its time constant", "[meter]")
{
    LevelMeter m;
    const auto x = sine (0.5, static_cast<int> (fs));
    m.addSamples (x.data(), x.data(), static_cast<int> (x.size()), fs);
    const auto before = m.rmsDb (0);

    const std::vector<float> silence (static_cast<size_t> (0.3 * fs), 0.0f);
    m.addSamples (silence.data(), silence.data(), static_cast<int> (silence.size()), fs);

    // One time constant on the mean square: -4.34 dB (10 log10 e^-1).
    CHECK_THAT (m.rmsDb (0), WithinAbs (before - 4.34, 0.2));
}

TEST_CASE ("Channels are independent and clipping latches until reset", "[meter]")
{
    LevelMeter m;
    const auto loud = sine (1.2, 480);
    const std::vector<float> quiet (480, 0.0f);
    m.addSamples (loud.data(), quiet.data(), 480, fs);
    m.update (0.0);

    CHECK (m.isClipped (0));
    CHECK_FALSE (m.isClipped (1));
    CHECK (m.peakDb (0) > 1.0);
    CHECK (m.peakDb (1) <= LevelMeter::floorDb + 1e-9);

    m.addSamples (quiet.data(), quiet.data(), 480, fs);
    m.update (5.0);
    CHECK (m.isClipped (0));   // still latched
    m.resetClip();
    CHECK_FALSE (m.isClipped (0));
}
