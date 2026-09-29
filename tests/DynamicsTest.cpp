#include "TestParameters.h"

#include "dsp/DynamicGainLaw.h"
#include "dsp/LevelDetector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace TestParameters;
using Law = DynamicGainLaw;

namespace
{
    constexpr double fs = 48000.0;

    double sine (int n, double f = 1000.0, double a = 1.0) { return a * std::sin (2.0 * std::numbers::pi * f * n / fs); }
}

//==============================================================================
TEST_CASE ("Detector attack and release are one-pole followers on the linear level", "[dynamics][detector]")
{
    // Decision 2026-09-29: classic follower on the rectified (Peak) or RMS level, converted to dB afterwards.
    LevelDetector d;
    d.prepare (fs);
    d.setMode (LevelDetector::Mode::peak);
    d.setTimes (10.0, 100.0);
    d.reset();
    CHECK_THAT (d.getLevelDb(), WithinAbs (LevelDetector::floorDb, 0.0));

    // A step from silence to a constant 1.0: after one attack time, 63.2 % of the way in linear level (-3.98 dB).
    const auto attackSamples = static_cast<int> (0.010 * fs);
    for (int n = 0; n < attackSamples; ++n)
        d.process (1.0);
    CHECK_THAT (d.getLevelDb(), WithinAbs (20.0 * std::log10 (1.0 - std::exp (-1.0)), 0.05));

    for (int n = 0; n < static_cast<int> (fs); ++n)
        d.process (1.0);
    CHECK_THAT (d.getLevelDb(), WithinAbs (0.0, 0.01));

    // Then down to 0.1: after one release time, 0.1 + 0.9 e^-1 in linear level (-7.30 dB).
    const auto releaseSamples = static_cast<int> (0.100 * fs);
    for (int n = 0; n < releaseSamples; ++n)
        d.process (0.1);
    CHECK_THAT (d.getLevelDb(), WithinAbs (20.0 * std::log10 (0.1 + 0.9 * std::exp (-1.0)), 0.05));
}

TEST_CASE ("After the signal stops the level falls 8.69 dB per release time", "[dynamics][detector]")
{
    for (auto mode : { LevelDetector::Mode::peak, LevelDetector::Mode::rms })
    {
        LevelDetector d;
        d.prepare (fs);
        d.setMode (mode);
        d.setTimes (1.0, 100.0);
        for (int n = 0; n < static_cast<int> (fs); ++n)
            d.process (0.5);
        const auto start = d.getLevelDb();

        // RMS: the 10 ms mean square still feeds the follower as it empties (amplitude time constant
        // ta = 20 ms), so after the first release time tr the level is (tr e^-1 - ta e^-tr/ta) / (tr - ta).
        for (int n = 0; n < static_cast<int> (0.1 * fs); ++n)
            d.process (0.0);
        const auto afterOne = d.getLevelDb();
        for (int n = 0; n < static_cast<int> (0.1 * fs); ++n)
            d.process (0.0);
        const auto afterTwo = d.getLevelDb();

        INFO ((mode == LevelDetector::Mode::peak ? "Peak" : "RMS"));
        const auto perReleaseTime = 20.0 * std::log10 (std::exp (1.0));   // 8.686 dB
        if (mode == LevelDetector::Mode::peak)
            CHECK_THAT (start - afterOne, WithinAbs (perReleaseTime, 0.01));
        else
            CHECK_THAT (start - afterOne, WithinAbs (-20.0 * std::log10 ((100.0 * std::exp (-1.0) - 20.0 * std::exp (-5.0)) / 80.0), 0.05));
        CHECK_THAT (afterOne - afterTwo, WithinAbs (perReleaseTime, 0.2));
    }
}

TEST_CASE ("At default times a steady tone reads close to its level", "[dynamics][detector]")
{
    // Default attack 10 ms, release 100 ms, 1 kHz and 100 Hz sines at -10 dBFS.
    // Peak reads below the peak because a 10 ms attack does not fully catch each cycle:
    // worst measured -1.16 dB (bound 1.3 dB). RMS reads the RMS (-13.01 dB).
    for (double f : { 100.0, 1000.0 })
    {
        LevelDetector peak, rms;
        for (auto* d : { &peak, &rms })
        {
            d->prepare (fs);
            d->setTimes (10.0, 100.0);
        }
        peak.setMode (LevelDetector::Mode::peak);
        rms.setMode (LevelDetector::Mode::rms);

        double lowest = 0.0, highest = -200.0;
        for (int n = 0; n < static_cast<int> (fs); ++n)
        {
            const auto x = sine (n, f, std::pow (10.0, -10.0 / 20.0));
            const auto level = peak.process (x);
            rms.process (x);
            if (n > static_cast<int> (0.8 * fs))
            {
                lowest = std::min (lowest, level);
                highest = std::max (highest, level);
            }
        }

        INFO (f << " Hz: Peak " << lowest << " .. " << highest << " dB, RMS " << rms.getLevelDb() << " dB");
        CHECK (lowest > -10.0 - 1.3);
        CHECK (highest <= -10.0 + 1e-6);
        CHECK_THAT (rms.getLevelDb(), WithinAbs (-13.01, f < 500.0 ? 0.3 : 0.1));
    }
}

TEST_CASE ("Peak reads a sine's peak, RMS its RMS", "[dynamics][detector]")
{
    LevelDetector peak, rms;
    for (auto* d : { &peak, &rms })
    {
        d->prepare (fs);
        d->setTimes (0.1, 1000.0);   // fast attack, slow release: holds the top
        d->reset();
    }
    peak.setMode (LevelDetector::Mode::peak);
    rms.setMode (LevelDetector::Mode::rms);

    for (int n = 0; n < static_cast<int> (fs); ++n)
    {
        const auto x = sine (n, 1000.0, 0.5);
        peak.process (x);
        rms.process (x);
    }

    CHECK_THAT (peak.getLevelDb(), WithinAbs (-6.02, 0.3));   // 0.5 peak
    CHECK_THAT (rms.getLevelDb(), WithinAbs (-9.03, 0.3));    // 0.5 / sqrt 2
}

TEST_CASE ("Detector reset returns to the floor", "[dynamics][detector]")
{
    LevelDetector d;
    d.prepare (fs);
    d.setTimes (1.0, 50.0);
    for (int n = 0; n < 4800; ++n)
        d.process (0.8);
    d.reset();
    CHECK_THAT (d.getLevelDb(), WithinAbs (LevelDetector::floorDb, 0.0));
}

//==============================================================================
TEST_CASE ("Range mode: nothing below the threshold, full range 12 dB above, smooth between", "[dynamics][law]")
{
    for (auto range : { -6.0, 9.0 })
    {
        INFO ("range " << range);
        CHECK_THAT (Law::gainChangeDb (Law::Mode::range, -40.0, -20.0, range, 2.0), WithinAbs (0.0, 0.0));
        CHECK_THAT (Law::gainChangeDb (Law::Mode::range, -20.0, -20.0, range, 2.0), WithinAbs (0.0, 1e-12));
        CHECK_THAT (Law::gainChangeDb (Law::Mode::range, -14.0, -20.0, range, 2.0), WithinAbs (range / 2.0, 1e-12));
        CHECK_THAT (Law::gainChangeDb (Law::Mode::range, -8.0, -20.0, range, 2.0), WithinAbs (range, 1e-12));
        CHECK_THAT (Law::gainChangeDb (Law::Mode::range, 0.0, -20.0, range, 2.0), WithinAbs (range, 1e-12));

        // Monotonic, with a flat start and end (smoothstep).
        double previous = 0.0;
        for (double level = -20.0; level <= -8.0; level += 0.25)
        {
            const auto change = Law::gainChangeDb (Law::Mode::range, level, -20.0, range, 2.0);
            CHECK (std::abs (change) >= std::abs (previous) - 1e-12);
            previous = change;
        }
        const auto h = 1e-4;
        CHECK (std::abs (Law::gainChangeDb (Law::Mode::range, -20.0 + h, -20.0, range, 2.0)) / h < 1e-2);
        CHECK (std::abs (range - Law::gainChangeDb (Law::Mode::range, -8.0 - h, -20.0, range, 2.0)) / h < 1e-2);
    }
}

TEST_CASE ("Ratio mode: compressor slope with a soft knee, capped at the range", "[dynamics][law]")
{
    // Well above the knee, the slope is exactly 1 - 1/ratio, in the direction of the range.
    for (auto ratio : { 2.0, 4.0, 10.0 })
    {
        const auto a = Law::gainChangeDb (Law::Mode::ratio, -10.0, -30.0, -24.0, ratio);
        const auto b = Law::gainChangeDb (Law::Mode::ratio, -5.0, -30.0, -24.0, ratio);
        INFO ("ratio " << ratio);
        CHECK_THAT (b - a, WithinAbs (-5.0 * (1.0 - 1.0 / ratio), 1e-9));
    }

    // Below the knee nothing; inside the knee the quadratic joins both sides continuously.
    CHECK_THAT (Law::gainChangeDb (Law::Mode::ratio, -34.0, -30.0, -12.0, 4.0), WithinAbs (0.0, 0.0));
    const auto kneeTop = Law::gainChangeDb (Law::Mode::ratio, -27.0, -30.0, -12.0, 4.0);
    CHECK_THAT (kneeTop, WithinAbs (-3.0 * 0.75, 1e-9));   // (over)(1 - 1/r) at the top of the knee
    CHECK_THAT (Law::gainChangeDb (Law::Mode::ratio, -30.0, -30.0, -12.0, 4.0), WithinAbs (-0.75 * 9.0 / 12.0, 1e-9));

    // Capped at the range, and the sign follows the range.
    CHECK_THAT (Law::gainChangeDb (Law::Mode::ratio, 0.0, -40.0, -6.0, 10.0), WithinAbs (-6.0, 1e-12));
    CHECK_THAT (Law::gainChangeDb (Law::Mode::ratio, 0.0, -40.0, 6.0, 10.0), WithinAbs (6.0, 1e-12));

    // Ratio 1:1 or range 0 does nothing.
    CHECK_THAT (Law::gainChangeDb (Law::Mode::ratio, 0.0, -40.0, -6.0, 1.0), WithinAbs (0.0, 1e-12));
    CHECK_THAT (Law::gainChangeDb (Law::Mode::ratio, 0.0, -40.0, 0.0, 4.0), WithinAbs (0.0, 1e-12));
}

//==============================================================================
TEST_CASE ("Every band has the nine dynamic parameters with their ranges and defaults", "[dynamics][parameters]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    for (int b = 1; b <= Parameters::numBands; ++b)
    {
        INFO ("band " << b);
        for (auto* field : Parameters::dynamicFields)
        {
            INFO ("field " << field);
            REQUIRE (p.getValueTreeState().getParameter (Parameters::id (b, field)) != nullptr);
            const auto isSpectral = juce::String (field) == "spectral";   // added in M9g
            CHECK (param (p, Parameters::id (b, field)).getVersionHint() == (isSpectral ? 5 : 4));
        }

        auto range = [&] (const char* f) { return param (p, Parameters::id (b, f)).getNormalisableRange(); };
        CHECK_THAT (range ("thresh").start, WithinAbs (-60.0f, 0.0f));
        CHECK_THAT (range ("thresh").end, WithinAbs (0.0f, 0.0f));
        CHECK_THAT (range ("range").start, WithinAbs (-24.0f, 0.0f));
        CHECK_THAT (range ("range").end, WithinAbs (24.0f, 0.0f));
        CHECK_THAT (range ("ratio").start, WithinAbs (1.0f, 0.0f));
        CHECK_THAT (range ("ratio").end, WithinAbs (20.0f, 0.0f));
        CHECK_THAT (range ("attack").start, WithinRel (0.1f));
        CHECK_THAT (range ("attack").end, WithinRel (200.0f));
        CHECK_THAT (range ("release").start, WithinRel (5.0f));
        CHECK_THAT (range ("release").end, WithinRel (2000.0f));

        CHECK_THAT (value (p, Parameters::id (b, "dyn")), WithinAbs (0.0f, 0.0f));
        CHECK_THAT (value (p, Parameters::id (b, "dynmode")), WithinAbs (0.0f, 0.0f));   // Range
        CHECK_THAT (value (p, Parameters::id (b, "thresh")), WithinAbs (-20.0f, 1e-4f));
        CHECK_THAT (value (p, Parameters::id (b, "range")), WithinAbs (-6.0f, 1e-4f));
        CHECK_THAT (value (p, Parameters::id (b, "ratio")), WithinRel (2.0f, 1e-4f));
        CHECK_THAT (value (p, Parameters::id (b, "attack")), WithinRel (10.0f, 1e-4f));
        CHECK_THAT (value (p, Parameters::id (b, "release")), WithinRel (100.0f, 1e-4f));
        CHECK_THAT (value (p, Parameters::id (b, "detector")), WithinAbs (0.0f, 0.0f));  // Peak
        CHECK_THAT (value (p, Parameters::id (b, "sidechain")), WithinAbs (0.0f, 0.0f));

        auto* mode = dynamic_cast<juce::AudioParameterChoice*> (&param (p, Parameters::id (b, "dynmode")));
        REQUIRE (mode != nullptr);
        CHECK (mode->choices == juce::StringArray { "Range", "Ratio" });
        auto* detector = dynamic_cast<juce::AudioParameterChoice*> (&param (p, Parameters::id (b, "detector")));
        REQUIRE (detector != nullptr);
        CHECK (detector->choices == juce::StringArray { "Peak", "RMS" });
    }
}

TEST_CASE ("Sessions saved before M7 load with dynamics off", "[dynamics][state]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    juce::XmlElement v3 ("ParametricEQ");
    v3.setAttribute ("stateVersion", 3);
    v3.setAttribute ("band1_used", 1);
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (v3, block);

    ParametricEQAudioProcessor p;
    set (p, "band1_dyn", 1.0f);   // must be reset by the load
    p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    CHECK_THAT (value (p, "band1_dyn"), WithinAbs (0.0f, 0.0f));
}

//==============================================================================
TEST_CASE ("reset() stops filter tails", "[dynamics][reset]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    setBand (p, 3, FilterType::bell, 100.0f, 18.0f, 12.0f, 3, true);   // long ringing tail
    p.setPlayConfigDetails (2, 2, fs, 512);
    p.prepareToPlay (fs, 512);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, 512);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);
    p.processBlock (buffer, midi);

    // Without reset the tail rings on ...
    juce::AudioBuffer<float> tail (2, 512);
    tail.clear();
    p.processBlock (tail, midi);
    REQUIRE (tail.getMagnitude (0, 0, 512) > 1e-3f);

    // ... after reset() silence stays silent.
    p.reset();
    tail.clear();
    p.processBlock (tail, midi);
    CHECK (juce::exactlyEqual (tail.getMagnitude (0, 0, 512), 0.0f));
    CHECK (juce::exactlyEqual (tail.getMagnitude (1, 0, 512), 0.0f));
}
