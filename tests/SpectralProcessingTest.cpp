#include "TestParameters.h"

#include "dsp/BandDesign.h"
#include "dsp/LinearPhaseEngine.h"
#include "dsp/SpectralDynamicsEngine.h"
#include "presets/Preset.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <numbers>

using Catch::Matchers::WithinAbs;
using namespace TestParameters;

namespace
{
    constexpr double fs = 48000.0;
    constexpr int spectralLatency = SpectralDynamicsEngine::latencySamples;

    void prepare (ParametricEQAudioProcessor& p, double rate = fs)
    {
        p.setPlayConfigDetails (2, 2, rate, 512);
        p.prepareToPlay (rate, 512);
    }

    /** A dynamic, spectral bell. */
    void spectralBell (ParametricEQAudioProcessor& p, int band, float f, float q, float thresh, float range, float gain = 0.0f)
    {
        setBand (p, band, FilterType::bell, f, gain, q, 3, true);
        set (p, Parameters::id (band, "dyn"), 1.0f);
        set (p, Parameters::id (band, "spectral"), 1.0f);
        set (p, Parameters::id (band, "thresh"), thresh);
        set (p, Parameters::id (band, "range"), range);
        set (p, Parameters::id (band, "attack"), 1.0f);
        set (p, Parameters::id (band, "release"), 50.0f);
    }

    bool settle (ParametricEQAudioProcessor& p)
    {
        juce::AudioBuffer<float> silence (2, 512);
        juce::MidiBuffer midi;
        const auto deadline = juce::Time::getMillisecondCounter() + 20000;
        while (! p.isPhaseModeSettled() && juce::Time::getMillisecondCounter() < deadline)
        {
            silence.clear();
            p.processBlock (silence, midi);
            juce::Thread::sleep (1);
        }
        for (int i = 0; i < 20; ++i)
        {
            silence.clear();
            p.processBlock (silence, midi);
        }
        return p.isPhaseModeSettled();
    }

    void run (ParametricEQAudioProcessor& p, juce::AudioBuffer<float>& signal)
    {
        juce::MidiBuffer midi;
        for (int start = 0; start < signal.getNumSamples(); start += 512)
        {
            juce::AudioBuffer<float> view (signal.getArrayOfWritePointers(), 2, start, std::min (512, signal.getNumSamples() - start));
            p.processBlock (view, midi);
        }
    }

    juce::AudioBuffer<float> tones (int numSamples, std::vector<std::pair<double, double>> list)
    {
        juce::AudioBuffer<float> b (2, numSamples);
        b.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (auto [f, db] : list)
                for (int i = 0; i < numSamples; ++i)
                    b.addSample (ch, i, static_cast<float> (std::pow (10.0, db / 20.0) * std::sin (2.0 * std::numbers::pi * f * i / fs)));
        return b;
    }

    double toneDb (const juce::AudioBuffer<float>& b, int channel, double f, int length = 9600)
    {
        const auto start = b.getNumSamples() - length;
        std::complex<double> sum;
        double windowSum = 0.0;
        for (int i = 0; i < length; ++i)
        {
            const auto w = 0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * i / length);
            windowSum += w;
            sum += w * static_cast<double> (b.getSample (channel, start + i)) * std::polar (1.0, -2.0 * std::numbers::pi * f * (start + i) / fs);
        }
        return 20.0 * std::log10 (std::max (1.0e-12, 2.0 * std::abs (sum) / windowSum));
    }
}

//==============================================================================
TEST_CASE ("Every band has a Spectral switch (off by default, version hint 5)", "[spectral][parameters]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    for (int b = 1; b <= Parameters::numBands; ++b)
    {
        auto& s = param (p, Parameters::id (b, "spectral"));
        CHECK (s.getVersionHint() == 5);
        CHECK_THAT (value (p, Parameters::id (b, "spectral")), WithinAbs (0.0f, 0.0f));
    }
    CHECK (p.getParameters().size() == 17 * Parameters::numBands + 3);
}

TEST_CASE ("Spectral latency is reported only while a band is spectral, on top of linear phase", "[spectral][latency]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    prepare (p);
    CHECK (p.getLatencySamples() == 0);

    spectralBell (p, 4, 2000.0f, 1.0f, -20.0f, -6.0f);
    p.refreshLatency();
    CHECK (p.getLatencySamples() == spectralLatency);

    p.setLinearPhase (true);
    CHECK (p.getLatencySamples() == LinearPhaseEngine::latencyFor (8192) + spectralLatency);

    set (p, "band4_spectral", 0.0f);
    p.refreshLatency();
    CHECK (p.getLatencySamples() == LinearPhaseEngine::latencyFor (8192));

    p.setLinearPhase (false);
    set (p, "band4_spectral", 1.0f);
    set (p, "band4_dyn", 0.0f);   // Spectral needs Dynamic
    p.refreshLatency();
    CHECK (p.getLatencySamples() == 0);
}

TEST_CASE ("With a spectral band the measured delay equals the reported latency", "[spectral][latency]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    for (bool linear : { false, true })
    {
        ParametricEQAudioProcessor p;
        spectralBell (p, 4, 2000.0f, 1.0f, 0.0f, -6.0f);   // threshold at 0 dBFS: no dynamic action
        p.setLinearPhase (linear);
        prepare (p);
        p.refreshLatency();
        REQUIRE (settle (p));

        const auto latency = p.getLatencySamples();
        INFO ((linear ? "linear phase" : "zero latency") << ", latency " << latency);
        CHECK (latency == (linear ? LinearPhaseEngine::latencyFor (8192) : 0) + spectralLatency);

        juce::AudioBuffer<float> x (2, latency + 4096);
        x.clear();
        x.setSample (0, 0, 1.0f);
        x.setSample (1, 0, 1.0f);
        run (p, x);
        int peak = 0;
        for (int n = 0; n < x.getNumSamples(); ++n)
            if (std::abs (x.getSample (0, n)) > std::abs (x.getSample (0, peak)))
                peak = n;
        CHECK (peak == latency);
        CHECK_THAT (x.getSample (0, latency), WithinAbs (1.0f, 1.0e-4f));
    }
}

TEST_CASE ("Through the processor, a spectral band cuts only the loud slice; its static gain still applies", "[spectral][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    SECTION ("per-slice action")
    {
        ParametricEQAudioProcessor p;
        spectralBell (p, 4, 2200.0f, 0.7f, -40.0f, -12.0f);
        prepare (p);
        p.refreshLatency();
        REQUIRE (settle (p));
        auto y = tones (static_cast<int> (fs), { { 2000.0, -10.0 }, { 2800.0, -55.0 } });
        run (p, y);
        INFO ("2 kHz " << toneDb (y, 0, 2000.0) << " dB, 2.8 kHz " << toneDb (y, 0, 2800.0) << " dB");
        CHECK (toneDb (y, 0, 2000.0) < -20.0);
        CHECK_THAT (toneDb (y, 0, 2800.0), WithinAbs (-55.0 + BandDesign::design (p.getBandSettings()[3], fs).magnitudeDb (2800.0, fs), 0.5));
    }

    SECTION ("static gain below threshold")
    {
        ParametricEQAudioProcessor p;
        spectralBell (p, 4, 1000.0f, 1.0f, 0.0f, -12.0f, 6.0f);   // +6 dB bell, never above threshold
        prepare (p);
        p.refreshLatency();
        REQUIRE (settle (p));
        auto y = tones (static_cast<int> (fs), { { 1000.0, -30.0 } });
        run (p, y);
        CHECK_THAT (toneDb (y, 0, 1000.0), WithinAbs (-24.0, 0.2));
    }
}

TEST_CASE ("A spectral band on side-chain ducks the main signal by the side-chain's slices", "[spectral][sidechain]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    spectralBell (p, 4, 2500.0f, 0.5f, -30.0f, -12.0f);
    set (p, "band4_sidechain", 1.0f);
    auto layout = p.getBusesLayout();
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    REQUIRE (p.setBusesLayout (layout));
    p.prepareToPlay (fs, 512);
    p.refreshLatency();
    REQUIRE (settle (p));

    const auto main = tones (static_cast<int> (fs), { { 2000.0, -40.0 }, { 3000.0, -40.0 } });
    const auto side = tones (static_cast<int> (fs), { { 2000.0, -6.0 } });
    juce::AudioBuffer<float> out (2, main.getNumSamples());
    juce::MidiBuffer midi;
    for (int start = 0; start < main.getNumSamples(); start += 512)
    {
        juce::AudioBuffer<float> block (4, 512);
        for (int ch = 0; ch < 2; ++ch)
        {
            block.copyFrom (ch, 0, main, ch, start, 512);
            block.copyFrom (2 + ch, 0, side, ch, start, 512);
        }
        p.processBlock (block, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, start, block, ch, 0, 512);
    }
    INFO ("2 kHz " << toneDb (out, 0, 2000.0) << " dB, 3 kHz " << toneDb (out, 0, 3000.0) << " dB");
    CHECK (toneDb (out, 0, 2000.0) < -46.0);
    CHECK_THAT (toneDb (out, 0, 3000.0), WithinAbs (-40.0, 0.5));
}

TEST_CASE ("Turning Spectral on and off during playback fades cleanly", "[spectral][switch]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    setBand (p, 4, FilterType::bell, 2000.0f, 0.0f, 1.0f, 3, true);
    set (p, "band4_dyn", 1.0f);
    prepare (p);

    juce::MidiBuffer midi;
    int sample = 0;
    bool finite = true;
    double largest = 0.0;
    float previous = 0.0f;
    auto play = [&] (int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> b (2, 512);
            for (int i = 0; i < 512; ++i, ++sample)
                for (int ch = 0; ch < 2; ++ch)
                    b.setSample (ch, i, 0.5f * static_cast<float> (std::sin (2.0 * std::numbers::pi * 440.0 * sample / fs)));
            p.processBlock (b, midi);
            for (int i = 0; i < 512; ++i)
            {
                const auto v = b.getSample (0, i);
                finite = finite && std::isfinite (v);
                largest = std::max (largest, static_cast<double> (std::abs (v - previous)));
                previous = v;
            }
        }
    };

    play (20);
    set (p, "band4_spectral", 1.0f);
    play (60);
    CHECK (p.isPhaseModeSettled());
    set (p, "band4_spectral", 0.0f);
    play (40);
    CHECK (p.isPhaseModeSettled());
    CHECK (finite);
    CHECK (largest < 0.2);
}

TEST_CASE ("Presets store the Spectral switch (format 3); older presets load with it off", "[spectral][preset]")
{
    CHECK (Preset::formatVersion == 3);

    Preset p;
    p.name = "Spec";
    p.bands[2].inUse = true;
    p.bands[2].dynamics.on = true;
    p.bands[2].dynamics.spectral = true;
    const auto parsed = Preset::fromXml (*p.toXml());
    REQUIRE (parsed.has_value());
    CHECK (parsed->bands[2].dynamics.spectral);
    CHECK (parsed->hasSameSettingsAs (p));

    auto changed = p;
    changed.bands[2].dynamics.spectral = false;
    CHECK_FALSE (changed.hasSameSettingsAs (p));

    const auto v2 = juce::parseXML (R"(<ParametricEQPreset formatVersion="2" name="Old" category="User" outputGain="0" autoGain="0" invert="0">
                                         <Band index="3" enabled="1" type="Bell" freq="800" gain="4" q="1.5" slope="3" channel="Stereo">
                                           <Dynamics on="1" mode="Range" threshold="-20" range="-6" ratio="2" attack="10" release="100" detector="Peak" sidechain="0"/>
                                         </Band>
                                       </ParametricEQPreset>)");
    REQUIRE (v2 != nullptr);
    const auto old = Preset::fromXml (*v2);
    REQUIRE (old.has_value());
    CHECK (old->bands[2].dynamics.on);
    CHECK_FALSE (old->bands[2].dynamics.spectral);
}
