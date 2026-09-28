#include "TestParameters.h"

#include "dsp/AutoGain.h"
#include "dsp/BandDesign.h"
#include "dsp/KWeighting.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <numbers>

using Catch::Matchers::WithinAbs;
using namespace TestParameters;

namespace
{
    constexpr double maxStep = 0.2;   // same step limit as the band tests

    void fillSine (juce::AudioBuffer<float>& b, double f, double amplitude, double fs, int start)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (ch, i, static_cast<float> (amplitude * std::sin (2.0 * std::numbers::pi * f * (start + i) / fs)));
    }

    double largestStep (const juce::AudioBuffer<float>& b)
    {
        double step = 0.0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 1; i < b.getNumSamples(); ++i)
                step = std::max (step, std::abs (static_cast<double> (b.getSample (ch, i)) - b.getSample (ch, i - 1)));
        return step;
    }

    void prepare (ParametricEQAudioProcessor& p, double fs)
    {
        p.setPlayConfigDetails (2, 2, fs, 512);
        p.prepareToPlay (fs, 512);
    }

    void process (ParametricEQAudioProcessor& p, juce::AudioBuffer<float>& signal)
    {
        juce::MidiBuffer midi;
        for (int start = 0; start < signal.getNumSamples(); start += 512)
        {
            const auto n = std::min (512, signal.getNumSamples() - start);
            juce::AudioBuffer<float> view (signal.getArrayOfWritePointers(), signal.getNumChannels(), start, n);
            p.processBlock (view, midi);
        }
    }

    /** Waits (up to 2 s) for the background Auto Gain thread to publish something other than 'previous'. */
    float waitForOffsetChange (ParametricEQAudioProcessor& p, float previous)
    {
        for (int i = 0; i < 200; ++i)
        {
            if (std::abs (p.getAutoGainOffsetDb() - previous) > 1e-4f)
                break;
            juce::Thread::sleep (10);
        }
        return p.getAutoGainOffsetDb();
    }

    /** Waits (up to 2 s) until the background thread has published the offset for exactly these
        settings, so a test never processes audio with an offset from half-set parameters. */
    bool waitForOffset (ParametricEQAudioProcessor& p, double fs)
    {
        std::array<BandSettings, 16> bands;
        auto& state = p.getValueTreeState();
        for (int band = 1; band <= Parameters::numBands; ++band)
        {
            auto raw = [&] (const char* field) { return state.getRawParameterValue (Parameters::id (band, field))->load(); };
            bands[static_cast<size_t> (band - 1)] = Parameters::toBandSettings (raw ("type"), raw ("freq"), raw ("gain"),
                                                                                raw ("q"), raw ("slope"), raw ("enabled"));
        }

        const auto expected = static_cast<float> (AutoGain::computeOffsetDb (bands, fs));

        for (int i = 0; i < 200; ++i)
        {
            if (std::abs (p.getAutoGainOffsetDb() - expected) < 1e-5f)
                return true;
            juce::Thread::sleep (10);
        }
        return false;
    }

    /** K-weighted pink-noise power ratio of a measured impulse response, in dB (same grid as AutoGain). */
    double measuredLoudnessChangeDb (const float* ir, int numSamples, double fs)
    {
        double num = 0.0, den = 0.0;

        for (int k = 0; k < AutoGain::numPoints; ++k)
        {
            const auto f = 20.0 * std::pow (1000.0, k / (AutoGain::numPoints - 1.0));
            const auto step = std::polar (1.0, -2.0 * std::numbers::pi * f / fs);
            std::complex<double> phasor { 1.0, 0.0 }, sum {};

            for (int n = 0; n < numSamples; ++n)
            {
                sum += static_cast<double> (ir[n]) * phasor;
                phasor *= step;
                if ((n & 1023) == 1023)
                    phasor /= std::abs (phasor);
            }

            const auto w = std::pow (10.0, KWeighting::magnitudeDb (f) / 10.0);
            num += w * std::norm (sum);
            den += w;
        }

        return 10.0 * std::log10 (num / den);
    }
}

TEST_CASE ("Output gain applies the set gain", "[output]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    set (p, Parameters::outputGain, 6.0f);
    prepare (p, 48000.0);

    juce::AudioBuffer<float> buffer (2, 4800);
    fillSine (buffer, 1000.0, 0.1, 48000.0, 0);
    process (p, buffer);

    CHECK_THAT (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, 2400, 2400) / 0.1f), WithinAbs (6.0, 0.01));
}

TEST_CASE ("Output gain changes are smoothed", "[output]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    prepare (p, 44100.0);

    juce::AudioBuffer<float> first (2, 2048), second (2, 4410);
    fillSine (first, 1000.0, 0.25, 44100.0, 0);
    process (p, first);

    set (p, Parameters::outputGain, 12.0f);
    fillSine (second, 1000.0, 0.25, 44100.0, 2048);
    process (p, second);

    CHECK (largestStep (second) < maxStep);
    CHECK_THAT (juce::Decibels::gainToDecibels (second.getMagnitude (0, 3000, 1410) / 0.25f), WithinAbs (12.0, 0.02));
}

TEST_CASE ("Phase invert flips the polarity exactly, and ramps when switched", "[output]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    SECTION ("on from the start: bit-exact negation")
    {
        ParametricEQAudioProcessor p;
        set (p, Parameters::outputInvert, 1.0f);
        prepare (p, 48000.0);

        juce::AudioBuffer<float> buffer (2, 1024), original;
        fillSine (buffer, 440.0, 0.5, 48000.0, 0);
        original.makeCopyOf (buffer);
        process (p, buffer);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 1024; ++i)
                REQUIRE (juce::exactlyEqual (buffer.getSample (ch, i), -original.getSample (ch, i)));
    }

    SECTION ("switched while running: no click, then exact")
    {
        ParametricEQAudioProcessor p;
        prepare (p, 44100.0);

        juce::AudioBuffer<float> warm (2, 1024);
        fillSine (warm, 1000.0, 0.25, 44100.0, 0);
        process (p, warm);

        set (p, Parameters::outputInvert, 1.0f);
        juce::AudioBuffer<float> ramp (2, 2048);
        fillSine (ramp, 1000.0, 0.25, 44100.0, 1024);
        process (p, ramp);
        CHECK (largestStep (ramp) < maxStep);

        juce::AudioBuffer<float> after (2, 512), original;
        fillSine (after, 1000.0, 0.25, 44100.0, 3072);
        original.makeCopyOf (after);
        process (p, after);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                REQUIRE (juce::exactlyEqual (after.getSample (ch, i), -original.getSample (ch, i)));
    }
}

TEST_CASE ("Auto Gain is computed off the audio thread", "[output][autogain]")
{
    // No processBlock calls at all: the background thread alone must publish the offset.
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    prepare (p, 48000.0);

    setBand (p, 5, FilterType::bell, 1000.0f, 9.0f, 1.0f, 3, true);
    const auto offset = waitForOffsetChange (p, 0.0f);
    REQUIRE (std::abs (offset) > 0.5f);   // a +9 dB bell must move the offset

    std::array<BandSettings, 16> bands;
    for (auto& b : bands) b.enabled = false;
    bands[4].type = FilterType::bell; bands[4].frequencyHz = 1000.0; bands[4].gainDb = 9.0; bands[4].q = 1.0; bands[4].enabled = true;

    CHECK_THAT (offset, WithinAbs (AutoGain::computeOffsetDb (bands, 48000.0), 1e-3));
}

TEST_CASE ("With Auto Gain on, measured K-weighted loudness matches the Auto Gain model", "[output][autogain]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double fs = 48000.0;
    constexpr int numSamples = 1 << 16;

    for (bool withCut : { false, true })
    {
        ParametricEQAudioProcessor p;
        setBand (p, 3, FilterType::bell, 1000.0f, 6.0f, 1.0f, 3, true);
        setBand (p, 2, FilterType::lowShelf, 100.0f, 4.0f, 0.71f, 3, true);
        setBand (p, 15, FilterType::highShelf, 9000.0f, -3.0f, 0.71f, 3, true);
        if (withCut)
            setBand (p, 16, FilterType::highCut, 12000.0f, 0.0f, 0.71f, 1, true);
        set (p, Parameters::autoGain, 1.0f);

        prepare (p, fs);
        REQUIRE (waitForOffset (p, fs));

        // Let the output gain settle on the published offset, then measure.
        juce::AudioBuffer<float> settle (2, 4800);
        settle.clear();
        process (p, settle);

        juce::AudioBuffer<float> ir (2, numSamples);
        ir.clear();
        ir.setSample (0, 0, 1.0f);
        ir.setSample (1, 0, 1.0f);
        process (p, ir);

        // Auto Gain makes sum w |T|^2 neutral (T = tone bands). With an excluded cut C also in the
        // path, the loudness change is sum w |T C|^2 / sum w |T|^2. Power weighting is not
        // separable, so this is not the cut's contribution on its own where T and C overlap.
        auto settingsOf = [] (FilterType t, double f, double g, double q, int slope)
        {
            BandSettings b;
            b.type = t; b.frequencyHz = f; b.gainDb = g; b.q = q; b.slopeIndex = slope;
            return b;
        };

        const SectionCascade tone[] { BandDesign::design (settingsOf (FilterType::bell, 1000.0, 6.0, 1.0, 3), fs),
                                      BandDesign::design (settingsOf (FilterType::lowShelf, 100.0, 4.0, 0.71, 3), fs),
                                      BandDesign::design (settingsOf (FilterType::highShelf, 9000.0, -3.0, 0.71, 3), fs) };
        const auto cut = BandDesign::design (settingsOf (FilterType::highCut, 12000.0, 0.0, 0.71, 1), fs);

        double withAll = 0.0, toneOnly = 0.0;
        for (int k = 0; k < AutoGain::numPoints; ++k)
        {
            const auto f = 20.0 * std::pow (1000.0, k / (AutoGain::numPoints - 1.0));
            const auto w = std::pow (10.0, KWeighting::magnitudeDb (f) / 10.0);

            double toneDb = 0.0;
            for (const auto& t : tone)
                toneDb += t.magnitudeDb (f, fs);

            const auto cutDb = withCut ? cut.magnitudeDb (f, fs) : 0.0;
            withAll  += w * std::pow (10.0, (toneDb + cutDb) / 10.0);
            toneOnly += w * std::pow (10.0, toneDb / 10.0);
        }
        const auto expected = 10.0 * std::log10 (withAll / toneOnly);

        INFO ("with cut=" << withCut << " offset=" << p.getAutoGainOffsetDb());
        CHECK_THAT (measuredLoudnessChangeDb (ir.getReadPointer (0), numSamples, fs), WithinAbs (expected, 0.05));
    }
}

TEST_CASE ("With Auto Gain off, the offset is not applied", "[output][autogain]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    setBand (p, 3, FilterType::bell, 1000.0f, 6.0f, 1.0f, 3, true);
    prepare (p, 48000.0);
    const auto offset = waitForOffsetChange (p, 0.0f);
    REQUIRE (std::abs (offset) > 0.5f);   // it was computed ...

    juce::AudioBuffer<float> buffer (2, 9600);
    fillSine (buffer, 100.0, 0.1, 48000.0, 0);   // far from the bell: the EQ is ~0 dB here
    process (p, buffer);

    // ... but not applied.
    CHECK_THAT (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, 4800, 4800) / 0.1f), WithinAbs (0.0, 0.1));
}
