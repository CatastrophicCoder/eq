#include "TestParameters.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using Catch::Matchers::WithinAbs;
using namespace TestParameters;

namespace
{
    constexpr double fs = 48000.0;

    struct Dyn
    {
        bool on = true;
        int mode = 0;          // 0 Range, 1 Ratio
        float thresh = -30.0f, range = -6.0f, ratio = 2.0f, attack = 1.0f, release = 50.0f;
        int detector = 0;      // 0 Peak, 1 RMS
        bool sidechain = false;
    };

    void setDynamics (ParametricEQAudioProcessor& p, int band, const Dyn& d)
    {
        set (p, Parameters::id (band, "dyn"), d.on ? 1.0f : 0.0f);
        set (p, Parameters::id (band, "dynmode"), static_cast<float> (d.mode));
        set (p, Parameters::id (band, "thresh"), d.thresh);
        set (p, Parameters::id (band, "range"), d.range);
        set (p, Parameters::id (band, "ratio"), d.ratio);
        set (p, Parameters::id (band, "attack"), d.attack);
        set (p, Parameters::id (band, "release"), d.release);
        set (p, Parameters::id (band, "detector"), static_cast<float> (d.detector));
        set (p, Parameters::id (band, "sidechain"), d.sidechain ? 1.0f : 0.0f);
    }

    void prepare (ParametricEQAudioProcessor& p)
    {
        p.setPlayConfigDetails (2, 2, fs, 512);
        p.prepareToPlay (fs, 512);
    }

    /** Stereo test signal: a sine per channel with its own level in dBFS (-inf for silence). */
    juce::AudioBuffer<float> sines (int numSamples, double f, double leftDb, double rightDb, int start = 0)
    {
        juce::AudioBuffer<float> b (2, numSamples);
        const double amp[] { std::isinf (leftDb) ? 0.0 : std::pow (10.0, leftDb / 20.0),
                             std::isinf (rightDb) ? 0.0 : std::pow (10.0, rightDb / 20.0) };
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < numSamples; ++i)
                b.setSample (ch, i, static_cast<float> (amp[ch] * std::sin (2.0 * std::numbers::pi * f * (start + i) / fs)));
        return b;
    }

    void run (ParametricEQAudioProcessor& p, juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* side = nullptr)
    {
        juce::MidiBuffer midi;
        const auto channels = side != nullptr ? 4 : 2;
        juce::AudioBuffer<float> block (channels, 512);

        for (int start = 0; start < main.getNumSamples(); start += 512)
        {
            const auto n = std::min (512, main.getNumSamples() - start);
            block.setSize (channels, n, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                block.copyFrom (ch, 0, main, ch, start, n);
            if (side != nullptr)
                for (int ch = 0; ch < 2; ++ch)
                    block.copyFrom (2 + ch, 0, *side, ch, start, n);

            p.processBlock (block, midi);

            for (int ch = 0; ch < 2; ++ch)
                main.copyFrom (ch, start, block, ch, 0, n);
        }
    }

    double levelDb (const juce::AudioBuffer<float>& b, int ch, int start, int n)
    {
        return juce::Decibels::gainToDecibels (static_cast<double> (b.getMagnitude (ch, start, n)), -200.0);
    }

    /** Bell at 1 kHz, 0 dB static, Q 1, dynamic. */
    void dynamicBell (ParametricEQAudioProcessor& p, int band, const Dyn& d, float f = 1000.0f, float q = 1.0f,
                      ChannelMode mode = ChannelMode::stereo)
    {
        setBand (p, band, FilterType::bell, f, 0.0f, q, 3, true);
        set (p, Parameters::id (band, "channel"), static_cast<float> (mode));
        setDynamics (p, band, d);
    }
}

//==============================================================================
TEST_CASE ("Range mode: a loud tone at the band's frequency is cut by the full range", "[dynamics][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    dynamicBell (p, 4, {});   // threshold -30, range -6
    prepare (p);

    auto x = sines (static_cast<int> (fs), 1000.0, -10.0, -10.0);
    run (p, x);

    for (int ch = 0; ch < 2; ++ch)
        CHECK_THAT (levelDb (x, ch, 38400, 9600), WithinAbs (-16.0, 0.3));
    CHECK_THAT (p.getLiveGainChangeDb (4, 0), WithinAbs (-6.0f, 0.3f));
    CHECK_THAT (p.getLiveGainChangeDb (4, 1), WithinAbs (-6.0f, 0.3f));
}

TEST_CASE ("Ratio mode and positive ranges act as their laws say", "[dynamics][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    SECTION ("ratio 4:1: the law applied to the detector's reading of the tone")
    {
        ParametricEQAudioProcessor p;
        Dyn d; d.mode = 1; d.ratio = 4.0f; d.range = -24.0f;
        dynamicBell (p, 4, d);
        prepare (p);
        auto x = sines (static_cast<int> (fs), 1000.0, -10.0, -10.0);
        run (p, x);

        // With a 1 ms attack the Peak follower does not fully catch each cycle, so a steady sine
        // reads a little below its peak (about -10.4 dB here). The band pass is unity at 1 kHz:
        // feed the raw tone to a detector.
        LevelDetector detector;
        detector.prepare (fs);
        detector.setTimes (d.attack, d.release);
        const auto tone = sines (static_cast<int> (fs), 1000.0, -10.0, -10.0);
        for (int i = 0; i < tone.getNumSamples(); ++i)
            detector.process (tone.getSample (0, i));
        const auto expected = DynamicGainLaw::gainChangeDb (DynamicGainLaw::Mode::ratio, detector.getLevelDb(), -30.0, -24.0, 4.0);
        INFO ("detector reads " << detector.getLevelDb() << " dB, law gives " << expected << " dB");

        CHECK (expected < -14.0);
        CHECK_THAT (p.getLiveGainChangeDb (4, 0), WithinAbs (expected, 0.3));
        CHECK_THAT (levelDb (x, 0, 38400, 9600), WithinAbs (-10.0 + expected, 0.3));
    }

    SECTION ("positive range boosts a loud tone")
    {
        ParametricEQAudioProcessor p;
        Dyn d; d.range = 6.0f;
        dynamicBell (p, 4, d);
        prepare (p);
        auto x = sines (static_cast<int> (fs), 1000.0, -10.0, -10.0);
        run (p, x);
        CHECK_THAT (levelDb (x, 0, 38400, 9600), WithinAbs (-4.0, 0.3));
    }
}

TEST_CASE ("Below the threshold a dynamic band sounds exactly like the static one", "[dynamics][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor dynamic, fixed;
    Dyn d; d.thresh = -20.0f;
    dynamicBell (dynamic, 4, d);
    setBand (fixed, 4, FilterType::bell, 1000.0f, 0.0f, 1.0f, 3, true);
    prepare (dynamic);
    prepare (fixed);

    auto a = sines (24000, 1000.0, -40.0, -40.0);
    auto b = sines (24000, 1000.0, -40.0, -40.0);
    run (dynamic, a);
    run (fixed, b);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 24000; ++i)
            REQUIRE_THAT (a.getSample (ch, i), WithinAbs (b.getSample (ch, i), 1e-6f));
}

TEST_CASE ("Faster attack reduces sooner; the reduction releases when the level drops", "[dynamics][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    auto timeToHalfReduction = [] (float attackMs)
    {
        ParametricEQAudioProcessor p;
        Dyn d; d.attack = attackMs; d.release = 50.0f;
        dynamicBell (p, 4, d);
        prepare (p);
        auto x = sines (static_cast<int> (fs), 1000.0, -10.0, -10.0);
        run (p, x);
        // First 1 ms window whose level is at least 3 dB down.
        for (int w = 0; w < 1000; ++w)
            if (levelDb (x, 0, w * 48, 48) < -13.0)
                return w;
        return 1000;
    };

    const auto fast = timeToHalfReduction (2.0f);
    const auto slow = timeToHalfReduction (50.0f);
    INFO ("fast " << fast << " ms, slow " << slow << " ms");
    CHECK (fast < slow);
    CHECK (fast < 20);
    CHECK (slow < 500);

    // Release: loud for 0.5 s, then quiet (below threshold): back to the static level.
    ParametricEQAudioProcessor p;
    Dyn d; d.attack = 2.0f; d.release = 50.0f;
    dynamicBell (p, 4, d);
    prepare (p);
    auto loud = sines (24000, 1000.0, -10.0, -10.0);
    run (p, loud);
    auto quiet = sines (48000, 1000.0, -50.0, -50.0, 24000);
    run (p, quiet);
    CHECK_THAT (levelDb (quiet, 0, 38400, 9600), WithinAbs (-50.0, 0.1));
    CHECK_THAT (p.getLiveGainChangeDb (4, 0), WithinAbs (0.0f, 0.05f));
}

TEST_CASE ("A tone far from the band's frequency does not trigger it", "[dynamics][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    dynamicBell (p, 4, {}, 5000.0f, 4.0f);
    prepare (p);

    auto x = sines (static_cast<int> (fs), 200.0, -10.0, -10.0);
    run (p, x);
    CHECK_THAT (levelDb (x, 0, 38400, 9600), WithinAbs (-10.0, 0.05));
    CHECK_THAT (p.getLiveGainChangeDb (4, 0), WithinAbs (0.0f, 0.05f));
}

TEST_CASE ("A dynamic shelf listens to its own side of the spectrum", "[dynamics][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    for (auto [type, triggerHz, otherHz] : { std::tuple { FilterType::highShelf, 8000.0, 150.0 },
                                             std::tuple { FilterType::lowShelf, 80.0, 6000.0 } })
    {
        for (auto [f, expectCut] : { std::pair { triggerHz, true }, std::pair { otherHz, false } })
        {
            ParametricEQAudioProcessor p;
            setBand (p, 2, type, type == FilterType::highShelf ? 2000.0f : 300.0f, 0.0f, 0.71f, 3, true);
            setDynamics (p, 2, {});
            prepare (p);
            auto x = sines (static_cast<int> (fs), f, -10.0, -10.0);
            run (p, x);
            INFO ((type == FilterType::highShelf ? "high shelf" : "low shelf") << " at " << f << " Hz");
            if (expectCut)
                CHECK (p.getLiveGainChangeDb (2, 0) < -5.0f);
            else
                CHECK_THAT (p.getLiveGainChangeDb (2, 0), WithinAbs (0.0f, 0.05f));
        }
    }
}

TEST_CASE ("Types that cannot be dynamic stay static with dynamics switched on", "[dynamics][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    setBand (p, 2, FilterType::notch, 1000.0f, 0.0f, 4.0f, 3, true);
    setDynamics (p, 2, {});
    prepare (p);
    auto x = sines (24000, 1000.0, -10.0, -10.0);
    run (p, x);
    CHECK_THAT (p.getLiveGainChangeDb (2, 0), WithinAbs (0.0f, 0.0f));
}

TEST_CASE ("Stereo dynamic bands act per channel; Mid/Side bands on their own part", "[dynamics][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    SECTION ("loud left, quiet right: only left is cut, right equals the static band")
    {
        ParametricEQAudioProcessor dynamic, fixed;
        dynamicBell (dynamic, 4, {});
        setBand (fixed, 4, FilterType::bell, 1000.0f, 0.0f, 1.0f, 3, true);
        prepare (dynamic);
        prepare (fixed);
        auto a = sines (static_cast<int> (fs), 1000.0, -10.0, -40.0);
        auto b = sines (static_cast<int> (fs), 1000.0, -10.0, -40.0);
        run (dynamic, a);
        run (fixed, b);

        CHECK_THAT (levelDb (a, 0, 38400, 9600), WithinAbs (-16.0, 0.3));
        for (int i = 0; i < static_cast<int> (fs); ++i)
            REQUIRE_THAT (a.getSample (1, i), WithinAbs (b.getSample (1, i), 1e-6f));
        CHECK_THAT (dynamic.getLiveGainChangeDb (4, 1), WithinAbs (0.0f, 0.05f));
    }

    SECTION ("a dynamic Side band ignores mid content and reacts to side content")
    {
        for (auto [rightDb, expectCut] : { std::pair { 1.0, false }, std::pair { -1.0, true } })
        {
            ParametricEQAudioProcessor p;
            dynamicBell (p, 4, {}, 1000.0f, 1.0f, ChannelMode::side);
            prepare (p);
            // Mid only: L = R. Side only: L = -R (made with a negative right amplitude).
            auto x = sines (static_cast<int> (fs), 1000.0, -10.0, -10.0);
            if (rightDb < 0.0)
                x.applyGain (1, 0, x.getNumSamples(), -1.0f);
            run (p, x);
            INFO ((expectCut ? "side content" : "mid content"));
            if (expectCut)
                CHECK (p.getLiveGainChangeDb (4, 0) < -5.0f);
            else
                CHECK_THAT (p.getLiveGainChangeDb (4, 0), WithinAbs (0.0f, 0.05f));
        }
    }
}

//==============================================================================
TEST_CASE ("The side-chain bus: optional, disabled by default, mono or stereo", "[dynamics][sidechain]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    REQUIRE (p.getBusCount (true) == 2);
    CHECK_FALSE (p.getBus (true, 1)->isEnabled());

    auto layout = p.getBusesLayout();
    for (auto set : { juce::AudioChannelSet::disabled(), juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo() })
    {
        layout.inputBuses.getReference (1) = set;
        INFO (set.getDescription());
        CHECK (p.checkBusesLayoutSupported (layout));
    }

    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
    CHECK_FALSE (p.checkBusesLayoutSupported (layout));
}

TEST_CASE ("A band set to side-chain reacts to the side-chain, not to the main signal", "[dynamics][sidechain]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    Dyn d; d.sidechain = true;
    dynamicBell (p, 4, d);

    auto layout = p.getBusesLayout();
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    REQUIRE (p.setBusesLayout (layout));
    p.prepareToPlay (fs, 512);

    // Quiet main (below threshold), loud side-chain: the main signal is cut.
    auto main = sines (static_cast<int> (fs), 1000.0, -40.0, -40.0);
    const auto side = sines (static_cast<int> (fs), 1000.0, -10.0, -10.0);
    run (p, main, &side);
    CHECK_THAT (levelDb (main, 0, 38400, 9600), WithinAbs (-46.0, 0.3));

    // Loud main, silent side-chain: nothing happens.
    ParametricEQAudioProcessor q;
    dynamicBell (q, 4, d);
    REQUIRE (q.setBusesLayout (layout));
    q.prepareToPlay (fs, 512);
    auto loudMain = sines (static_cast<int> (fs), 1000.0, -10.0, -10.0);
    const auto silent = sines (static_cast<int> (fs), 1000.0, -INFINITY, -INFINITY);
    run (q, loudMain, &silent);
    CHECK_THAT (levelDb (loudMain, 0, 38400, 9600), WithinAbs (-10.0, 0.1));
}

TEST_CASE ("With no side-chain connected, a side-chain band uses its own signal", "[dynamics][sidechain]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    Dyn d; d.sidechain = true;
    dynamicBell (p, 4, d);
    prepare (p);   // side-chain bus left disabled

    auto x = sines (static_cast<int> (fs), 1000.0, -10.0, -10.0);
    run (p, x);
    CHECK_THAT (levelDb (x, 0, 38400, 9600), WithinAbs (-16.0, 0.3));
}

//==============================================================================
TEST_CASE ("Dynamic bands stay clean: no NaN, no clicks on bursts and sweeps", "[dynamics][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    SECTION ("tone bursts with a fast attack and release")
    {
        ParametricEQAudioProcessor p;
        Dyn d; d.attack = 0.1f; d.release = 5.0f; d.range = -12.0f;
        dynamicBell (p, 4, d);
        prepare (p);

        // 1 kHz bursts, 50 ms on / 50 ms off, with 1 ms ramps so the input itself has no steps.
        auto x = sines (static_cast<int> (fs), 1000.0, -6.0, -6.0);
        for (int i = 0; i < x.getNumSamples(); ++i)
        {
            const auto phase = i % 4800;
            const auto gate = phase < 2400 ? std::min (1.0, std::min (phase, 2400 - phase) / 48.0) : 0.0;
            for (int ch = 0; ch < 2; ++ch)
                x.setSample (ch, i, static_cast<float> (x.getSample (ch, i) * gate));
        }
        run (p, x);

        double largest = 0.0;
        bool finite = true;
        for (int i = 1; i < x.getNumSamples(); ++i)
        {
            finite = finite && std::isfinite (x.getSample (0, i));
            largest = std::max (largest, std::abs (static_cast<double> (x.getSample (0, i)) - x.getSample (0, i - 1)));
        }
        CHECK (finite);
        CHECK (largest < 0.2);
    }

    SECTION ("frequency sweep with dynamics working")
    {
        ParametricEQAudioProcessor p;
        Dyn d; d.attack = 1.0f; d.release = 20.0f; d.range = -12.0f;
        dynamicBell (p, 4, d, 20.0f, 2.0f);
        prepare (p);

        auto x = sines (static_cast<int> (fs), 1000.0, -10.0, -10.0);
        juce::MidiBuffer midi;
        for (int start = 0; start < x.getNumSamples(); start += 512)
        {
            set (p, "band4_freq", static_cast<float> (20.0 * std::pow (1000.0, static_cast<double> (start) / x.getNumSamples())));
            juce::AudioBuffer<float> view (x.getArrayOfWritePointers(), 2, start, std::min (512, x.getNumSamples() - start));
            p.processBlock (view, midi);
        }

        double largest = 0.0;
        for (int i = 1; i < x.getNumSamples(); ++i)
            largest = std::max (largest, std::abs (static_cast<double> (x.getSample (0, i)) - x.getSample (0, i - 1)));
        CHECK (std::isfinite (x.getMagnitude (0, 0, x.getNumSamples())));
        CHECK (largest < 0.2);
    }
}

TEST_CASE ("reset() clears the detector envelopes", "[dynamics][reset]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    Dyn d; d.release = 2000.0f;   // would otherwise hold the cut for seconds
    dynamicBell (p, 4, d);
    prepare (p);

    auto loud = sines (24000, 1000.0, -10.0, -10.0);
    run (p, loud);
    REQUIRE (p.getLiveGainChangeDb (4, 0) < -5.0f);

    p.reset();
    auto quiet = sines (4800, 1000.0, -50.0, -50.0);
    run (p, quiet);
    CHECK_THAT (levelDb (quiet, 0, 2400, 2400), WithinAbs (-50.0, 0.1));
}

TEST_CASE ("16 dynamic stereo bells at 96 kHz run faster than real time", "[dynamics][cpu]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double rate = 96000.0;
    ParametricEQAudioProcessor p;
    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        Dyn d; d.thresh = -40.0f;
        dynamicBell (p, band, d, static_cast<float> (40.0 * std::pow (2.0, (band - 1) * 0.6)), 2.0f);
    }
    p.setPlayConfigDetails (2, 2, rate, 512);
    p.prepareToPlay (rate, 512);

    juce::AudioBuffer<float> x (2, static_cast<int> (rate));
    juce::Random random (8);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < x.getNumSamples(); ++i)
            x.setSample (ch, i, random.nextFloat() - 0.5f);

    const auto start = juce::Time::getMillisecondCounterHiRes();
    juce::MidiBuffer midi;
    for (int s = 0; s < x.getNumSamples(); s += 512)
    {
        juce::AudioBuffer<float> view (x.getArrayOfWritePointers(), 2, s, std::min (512, x.getNumSamples() - s));
        p.processBlock (view, midi);
    }
    const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - start;

    WARN ("16 dynamic stereo bells at 96 kHz: 1 s of audio in " << elapsedMs << " ms (" << 1000.0 / elapsedMs << "x real time)");
    CHECK (elapsedMs < 1000.0);
}
