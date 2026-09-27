#include "dsp/MatchedPeakingDesign.h"
#include "dsp/PeakingBand.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double toleranceDb = 0.1;

    // Largest sample-to-sample step allowed in the modulation tests. A clean 1 kHz sine at the
    // highest output these tests can produce (0.25 input, +12 dB = 1.0) steps by at most
    // 2 pi 1000 / 44100 = 0.142 per sample; 0.2 leaves about 40 % headroom.
    constexpr double maxStep = 0.2;

    /** Magnitude in dB of the DFT of x evaluated at exactly frequencyHz. */
    double dftMagnitudeDb (const float* x, int numSamples, double frequencyHz, double sampleRate)
    {
        const auto w = 2.0 * std::numbers::pi * frequencyHz / sampleRate;
        std::complex<double> sum {};

        for (int n = 0; n < numSamples; ++n)
            sum += static_cast<double> (x[n]) * std::polar (1.0, -w * n);

        return 20.0 * std::log10 (std::abs (sum));
    }

    struct SweepResult
    {
        bool allFinite = true;
        double largestStep = 0.0;
    };

    SweepResult measure (const juce::AudioBuffer<float>& buffer)
    {
        SweepResult r;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* x = buffer.getReadPointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                r.allFinite = r.allFinite && std::isfinite (x[i]);

                if (i > 0)
                    r.largestStep = std::max (r.largestStep, std::abs (static_cast<double> (x[i]) - x[i - 1]));
            }
        }

        return r;
    }

    void fillSine (juce::AudioBuffer<float>& buffer, double frequencyHz, double amplitude, double sampleRate, int startSample)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, static_cast<float> (amplitude * std::sin (2.0 * std::numbers::pi * frequencyHz
                                                                                   * (startSample + i) / sampleRate)));
    }
}

TEST_CASE ("PeakingBand loads the target coefficients on prepare", "[band]")
{
    PeakingBand band;
    band.setTargets (2500.0, -7.5, 2.0);
    band.prepare (48000.0, 2);

    const auto expected = MatchedPeakingDesign::design (2500.0, -7.5, 2.0, 48000.0);
    const auto& actual = band.getCurrentCoefficients();

    CHECK_THAT (actual.b0, WithinAbs (expected.b0, 1e-12));
    CHECK_THAT (actual.b1, WithinAbs (expected.b1, 1e-12));
    CHECK_THAT (actual.b2, WithinAbs (expected.b2, 1e-12));
    CHECK_THAT (actual.a1, WithinAbs (expected.a1, 1e-12));
    CHECK_THAT (actual.a2, WithinAbs (expected.a2, 1e-12));
}

TEST_CASE ("PeakingBand measured impulse response matches the design", "[band]")
{
    // Long enough for the slowest-decaying case (100 Hz, Q 4, +18 dB at 96 kHz) to fall below
    // float resolution.
    constexpr int numSamples = 1 << 18;

    struct Case { double sampleRate, centreHz; };
    const Case cases[] { { 44100.0, 100.0 }, { 44100.0, 1000.0 }, { 44100.0, 5000.0 },
                         { 48000.0, 100.0 }, { 48000.0, 1000.0 }, { 48000.0, 5000.0 },
                         { 96000.0, 100.0 }, { 96000.0, 1000.0 }, { 96000.0, 5000.0 },
                         { 192000.0, 10000.0 }, { 192000.0, 20000.0 } };

    for (const auto& c : cases)
    {
        for (auto gainDb : { -18.0, -6.0, 6.0, 18.0 })
        {
            for (auto q : { 0.5, 4.0 })
            {
                PeakingBand band;
                band.setTargets (c.centreHz, gainDb, q);
                band.prepare (c.sampleRate, 2);

                juce::AudioBuffer<float> buffer (2, numSamples);
                buffer.clear();
                buffer.setSample (0, 0, 1.0f);
                buffer.setSample (1, 0, 1.0f);
                band.process (buffer);

                const auto design = MatchedPeakingDesign::design (c.centreHz, gainDb, q, c.sampleRate);

                for (auto f : { c.centreHz / 2.0, c.centreHz, c.centreHz * 2.0 })
                {
                    if (f >= c.sampleRate / 2.0)
                        continue;

                    INFO ("fs=" << c.sampleRate << " f0=" << c.centreHz << " gain=" << gainDb
                                << " Q=" << q << " f=" << f);

                    const auto expected = design.magnitudeDb (f, c.sampleRate);

                    for (int ch = 0; ch < 2; ++ch)
                        CHECK_THAT (dftMagnitudeDb (buffer.getReadPointer (ch), numSamples, f, c.sampleRate),
                                    WithinAbs (expected, toleranceDb));
                }

                INFO ("fs=" << c.sampleRate << " f0=" << c.centreHz << " gain=" << gainDb << " Q=" << q);
                CHECK_THAT (dftMagnitudeDb (buffer.getReadPointer (0), numSamples, c.centreHz, c.sampleRate),
                            WithinAbs (MatchedPeakingDesign::analogMagnitudeDb (c.centreHz, c.centreHz, gainDb, q),
                                       toleranceDb));
            }
        }
    }
}

TEST_CASE ("PeakingBand survives a 20 Hz to 20 kHz sweep in one second", "[band]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    constexpr int blockSize = 64;
    const auto totalSamples = static_cast<int> (sampleRate);

    PeakingBand band;
    band.setTargets (20.0, 12.0, 4.0);
    band.prepare (sampleRate, 2);

    juce::AudioBuffer<float> output (2, totalSamples);
    juce::AudioBuffer<float> block (2, blockSize);

    for (int start = 0; start < totalSamples; start += blockSize)
    {
        const auto n = std::min (blockSize, totalSamples - start);
        block.setSize (2, n, false, false, true);

        // Log sweep, as host automation would send it: one new target per block.
        const auto t = static_cast<double> (start) / totalSamples;
        band.setTargets (20.0 * std::pow (1000.0, t), 12.0, 4.0);

        fillSine (block, 1000.0, 0.25, sampleRate, start);
        band.process (block);

        for (int ch = 0; ch < 2; ++ch)
            output.copyFrom (ch, start, block, ch, 0, n);
    }

    INFO ("fs=" << sampleRate);
    const auto result = measure (output);
    CHECK (result.allFinite);
    CHECK (result.largestStep < maxStep);
}

TEST_CASE ("PeakingBand ramps a gain jump over the smoothing time", "[band]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const auto rampSamples = static_cast<int> (std::ceil (PeakingBand::rampSeconds * sampleRate));

    PeakingBand band;
    band.setTargets (1000.0, 0.0, 1.0);
    band.prepare (sampleRate, 2);

    band.setTargets (1000.0, 12.0, 1.0);

    // Halfway through the ramp the band is somewhere between 0 and +12 dB at f0.
    juce::AudioBuffer<float> firstHalf (2, rampSamples / 2);
    fillSine (firstHalf, 1000.0, 0.25, sampleRate, 0);
    band.process (firstHalf);

    const auto midGain = band.getCurrentCoefficients().magnitudeDb (1000.0, sampleRate);
    INFO ("fs=" << sampleRate << " mid-ramp gain=" << midGain);
    CHECK (midGain > 1.0);
    CHECK (midGain < 11.0);

    // After the ramp plus one sub-block the band sits exactly on the target.
    const auto remaining = rampSamples - rampSamples / 2 + PeakingBand::subBlockSize;
    juce::AudioBuffer<float> secondHalf (2, remaining);
    fillSine (secondHalf, 1000.0, 0.25, sampleRate, rampSamples / 2);
    band.process (secondHalf);

    const auto expected = MatchedPeakingDesign::design (1000.0, 12.0, 1.0, sampleRate);
    const auto& actual = band.getCurrentCoefficients();
    CHECK_THAT (actual.b0, WithinAbs (expected.b0, 1e-9));
    CHECK_THAT (actual.b1, WithinAbs (expected.b1, 1e-9));
    CHECK_THAT (actual.b2, WithinAbs (expected.b2, 1e-9));
    CHECK_THAT (actual.a1, WithinAbs (expected.a1, 1e-9));
    CHECK_THAT (actual.a2, WithinAbs (expected.a2, 1e-9));

    CHECK (measure (firstHalf).largestStep < maxStep);
    CHECK (measure (secondHalf).largestStep < maxStep);
}

TEST_CASE ("PeakingBand reset clears the filter state", "[band]")
{
    PeakingBand band;
    band.setTargets (200.0, 18.0, 4.0);
    band.prepare (48000.0, 2);

    juce::AudioBuffer<float> buffer (2, 512);
    fillSine (buffer, 200.0, 0.5, 48000.0, 0);
    band.process (buffer);

    band.reset();
    buffer.clear();
    band.process (buffer);

    CHECK (juce::exactlyEqual (buffer.getMagnitude (0, buffer.getNumSamples()), 0.0f));
}
