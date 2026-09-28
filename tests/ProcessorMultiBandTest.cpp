#include "TestParameters.h"

#include "dsp/BandDesign.h"
#include "dsp/CutSlope.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <numbers>

using Catch::Matchers::WithinAbs;
using namespace TestParameters;

namespace
{
    double dftMagnitudeDb (const float* x, int numSamples, double frequencyHz, double sampleRate)
    {
        const auto step = std::polar (1.0, -2.0 * std::numbers::pi * frequencyHz / sampleRate);
        std::complex<double> phasor { 1.0, 0.0 }, sum {};

        for (int n = 0; n < numSamples; ++n)
        {
            sum += static_cast<double> (x[n]) * phasor;
            phasor *= step;

            if ((n & 1023) == 1023)
                phasor /= std::abs (phasor);
        }

        return 20.0 * std::log10 (std::abs (sum));
    }

    /** Feeds a buffer through processBlock in host-sized blocks. */
    void processInBlocks (ParametricEQAudioProcessor& p, juce::AudioBuffer<float>& signal, int blockSize)
    {
        juce::MidiBuffer midi;

        for (int start = 0; start < signal.getNumSamples(); start += blockSize)
        {
            const auto n = std::min (blockSize, signal.getNumSamples() - start);
            juce::AudioBuffer<float> view (signal.getArrayOfWritePointers(), signal.getNumChannels(), start, n);
            p.processBlock (view, midi);
        }
    }
}

TEST_CASE ("A new instance passes audio through bit-exactly", "[multiband]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 4096), original;
    juce::Random random (7);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 4096; ++i)
            buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);
    original.makeCopyOf (buffer);

    processInBlocks (p, buffer, 512);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 4096; ++i)
            REQUIRE (juce::exactlyEqual (buffer.getSample (ch, i), original.getSample (ch, i)));
}

TEST_CASE ("Bands in series: measured response equals the sum of the band designs", "[multiband]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    for (auto fs : { 44100.0, 48000.0, 96000.0 })
    {
        ParametricEQAudioProcessor p;
        setBand (p, 1,  FilterType::lowCut,    40.0f,    0.0f,  0.71f, 3, true);
        setBand (p, 4,  FilterType::bell,      400.0f,   6.0f,  2.0f,  3, true);
        setBand (p, 9,  FilterType::notch,     2200.0f,  0.0f,  4.0f,  3, true);
        setBand (p, 15, FilterType::highShelf, 8000.0f, -4.0f,  0.71f, 3, true);
        setBand (p, 16, FilterType::highCut,   18000.0f, 0.0f,  0.71f, 1, false);   // disabled: must not count

        p.setPlayConfigDetails (2, 2, fs, 512);
        p.prepareToPlay (fs, 512);

        constexpr int numSamples = 1 << 17;
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        processInBlocks (p, buffer, 512);

        auto settingsOf = [] (FilterType t, double f, double g, double q, int slope)
        {
            BandSettings s;
            s.type = t; s.frequencyHz = f; s.gainDb = g; s.q = q; s.slopeIndex = slope;
            return s;
        };

        const SectionCascade designs[] {
            BandDesign::design (settingsOf (FilterType::lowCut, 40.0, 0.0, 0.71, 3), fs),
            BandDesign::design (settingsOf (FilterType::bell, 400.0, 6.0, 2.0, 3), fs),
            BandDesign::design (settingsOf (FilterType::notch, 2200.0, 0.0, 4.0, 3), fs),
            BandDesign::design (settingsOf (FilterType::highShelf, 8000.0, -4.0, 0.71, 3), fs) };

        for (auto f : { 30.0, 60.0, 200.0, 400.0, 1000.0, 1800.0, 3000.0, 8000.0, 16000.0 })
        {
            double expected = 0.0;
            for (const auto& d : designs)
                expected += d.magnitudeDb (f, fs);

            INFO ("fs=" << fs << " f=" << f);
            for (int ch = 0; ch < 2; ++ch)
                CHECK_THAT (dftMagnitudeDb (buffer.getReadPointer (ch), numSamples, f, fs), WithinAbs (expected, 0.1));
        }
    }
}

TEST_CASE ("16 Brickwall bands at 96 kHz run faster than real time", "[multiband][cpu]")
{
    // Worst case per band: 16 sections (Brickwall). Always-on check (decision 2026-09-28):
    // must hold in whatever build runs it, Debug included.
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double fs = 96000.0;
    constexpr int blockSize = 512;

    ParametricEQAudioProcessor p;
    for (int band = 1; band <= Parameters::numBands; ++band)
        setBand (p, band, band % 2 == 0 ? FilterType::lowCut : FilterType::highCut,
                 20.0f * std::pow (1000.0f, (band - 1) / 15.0f), 0.0f, 0.71f, CutSlope::brickwallIndex, true);

    p.setPlayConfigDetails (2, 2, fs, blockSize);
    p.prepareToPlay (fs, blockSize);

    juce::AudioBuffer<float> buffer (2, static_cast<int> (fs));
    juce::Random random (3);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

    const auto start = juce::Time::getMillisecondCounterHiRes();
    processInBlocks (p, buffer, blockSize);
    const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - start;

    const auto realTimeFactor = 1000.0 / elapsedMs;
    WARN ("16 x Brickwall at 96 kHz stereo: 1 s of audio in " << elapsedMs << " ms (" << realTimeFactor << "x real time)");
    CHECK (elapsedMs < 1000.0);

    for (int ch = 0; ch < 2; ++ch)
        CHECK (std::isfinite (buffer.getMagnitude (ch, 0, buffer.getNumSamples())));
}
