#include "TestParameters.h"

#include "dsp/BandDesign.h"
#include "dsp/LinearPhaseDesigner.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <numbers>

using Catch::Matchers::WithinAbs;
using namespace TestParameters;

namespace
{
    void prepare (ParametricEQAudioProcessor& p, double fs)
    {
        p.setPlayConfigDetails (2, 2, fs, 512);
        p.prepareToPlay (fs, 512);
    }

    /** Processes silence until the phase mode has settled (filter loaded, fade finished). */
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

        // A few more blocks so any trailing crossfade inside the convolution has finished.
        for (int i = 0; i < 20; ++i)
        {
            silence.clear();
            p.processBlock (silence, midi);
        }

        return p.isPhaseModeSettled();
    }

    /** Runs a whole signal through in 512-sample blocks (both channels in place). */
    void run (ParametricEQAudioProcessor& p, juce::AudioBuffer<float>& signal)
    {
        juce::MidiBuffer midi;
        for (int start = 0; start < signal.getNumSamples(); start += 512)
        {
            juce::AudioBuffer<float> view (signal.getArrayOfWritePointers(), 2, start, std::min (512, signal.getNumSamples() - start));
            p.processBlock (view, midi);
        }
    }

    /** Stereo impulse response of the processor: an impulse on the chosen input channel. */
    juce::AudioBuffer<float> impulseResponse (ParametricEQAudioProcessor& p, int length, int inputChannel = -1)
    {
        juce::AudioBuffer<float> x (2, length);
        x.clear();
        for (int ch = 0; ch < 2; ++ch)
            if (inputChannel < 0 || inputChannel == ch)
                x.setSample (ch, 0, 1.0f);
        run (p, x);
        return x;
    }

    /** Magnitude of a response at one frequency, with a known delay removed (DTFT, double). */
    double magnitudeDb (const juce::AudioBuffer<float>& h, int channel, double f, double fs)
    {
        std::complex<double> sum;
        for (int n = 0; n < h.getNumSamples(); ++n)
            sum += static_cast<double> (h.getSample (channel, n)) * std::polar (1.0, -2.0 * std::numbers::pi * f / fs * n);
        return 20.0 * std::log10 (std::max (1.0e-12, std::abs (sum)));
    }

    double largestStep (const juce::AudioBuffer<float>& x, bool& finite)
    {
        double largest = 0.0;
        finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 1; i < x.getNumSamples(); ++i)
            {
                finite = finite && std::isfinite (x.getSample (ch, i));
                largest = std::max (largest, std::abs (static_cast<double> (x.getSample (ch, i)) - x.getSample (ch, i - 1)));
            }
        return largest;
    }
}

//==============================================================================
TEST_CASE ("Phase mode and length are saved with the session; older sessions load in Zero latency", "[linearphase][state]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    ParametricEQAudioProcessor fresh;
    CHECK_FALSE (fresh.isLinearPhase());
    CHECK (fresh.getLinearPhaseLength() == 0);
    CHECK (fresh.getLatencySamples() == 0);
    CHECK (fresh.getValueTreeState().getParameter ("phase_mode") == nullptr);   // not a host parameter

    juce::MemoryBlock saved;
    {
        ParametricEQAudioProcessor source;
        source.setLinearPhase (true);
        source.setLinearPhaseLength (2);
        CHECK (source.getLatencySamples() == 32768 / 2);
        source.getStateInformation (saved);
    }

    ParametricEQAudioProcessor target;
    target.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    CHECK (target.isLinearPhase());
    CHECK (target.getLinearPhaseLength() == 2);
    CHECK (target.getLatencySamples() == 16384);

    // A state from before M8 (version 3) has neither property.
    juce::XmlElement v3 ("ParametricEQ");
    v3.setAttribute ("stateVersion", 3);
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary (v3, old);
    target.setStateInformation (old.getData(), static_cast<int> (old.getSize()));
    CHECK_FALSE (target.isLinearPhase());
    CHECK (target.getLatencySamples() == 0);

    // Setting the length alone does not report latency in Zero latency mode.
    target.setLinearPhaseLength (1);
    CHECK (target.getLatencySamples() == 0);
}

TEST_CASE ("Reported latency equals the measured delay; the impulse response is symmetric", "[linearphase][latency]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int length = 0; length < 3; ++length)
        {
            ParametricEQAudioProcessor p;
            setBand (p, 3, FilterType::bell, 1500.0f, 6.0f, 1.5f, 3, true);
            p.setLinearPhase (true);
            p.setLinearPhaseLength (length);
            prepare (p, fs);
            REQUIRE (settle (p));

            const auto taps = LinearPhaseDesigner::tapCounts[static_cast<size_t> (length)];
            const auto latency = p.getLatencySamples();
            INFO (fs << " Hz, " << taps << " taps");
            CHECK (latency == taps / 2);

            const auto h = impulseResponse (p, taps + 1024, 0);
            int peak = 0;
            for (int n = 0; n < h.getNumSamples(); ++n)
                if (std::abs (h.getSample (0, n)) > std::abs (h.getSample (0, peak)))
                    peak = n;
            CHECK (peak == latency);

            double asymmetry = 0.0;
            for (int j = 1; j < latency; ++j)
                asymmetry = std::max (asymmetry, static_cast<double> (std::abs (h.getSample (0, latency + j) - h.getSample (0, latency - j))));
            CHECK (asymmetry < 1.0e-5);

            // Nothing on the other channel, nothing after the filter ends.
            CHECK (h.getMagnitude (1, 0, h.getNumSamples()) < 1.0e-6f);
            CHECK (h.getMagnitude (0, taps, 1024) < 1.0e-6f);
        }
}

TEST_CASE ("A flat EQ in Linear phase mode is an exact delay", "[linearphase][latency]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    p.setLinearPhase (true);
    prepare (p, 48000.0);
    REQUIRE (settle (p));

    juce::AudioBuffer<float> x (2, 20000);
    juce::Random random (6);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < x.getNumSamples(); ++i)
            x.setSample (ch, i, random.nextFloat() - 0.5f);
    auto y = x;
    run (p, y);

    const auto latency = p.getLatencySamples();
    for (int ch = 0; ch < 2; ++ch)
        for (int i = latency; i < x.getNumSamples(); ++i)
            REQUIRE_THAT (y.getSample (ch, i), WithinAbs (x.getSample (ch, i - latency), 1.0e-5f));
}

TEST_CASE ("Linear-phase magnitude nulls against Zero latency within the design bounds", "[linearphase][null]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double fs = 48000.0;
    const auto length = 1;   // 16384 taps: resolution 32 fs / N = 94 Hz

    auto setUp = [] (ParametricEQAudioProcessor& p)
    {
        setBand (p, 1, FilterType::lowCut, 40.0f, 0.0f, 0.71f, 1, true);      // 12 dB/oct, below the checked range
        setBand (p, 4, FilterType::bell, 1000.0f, 6.0f, 1.0f, 3, true);
        setBand (p, 6, FilterType::bell, 3000.0f, -4.0f, 2.0f, 3, true);
        set (p, "band6_channel", static_cast<float> (ChannelMode::left));
        setBand (p, 9, FilterType::highShelf, 8000.0f, 3.0f, 0.71f, 3, true);
        set (p, "band9_channel", static_cast<float> (ChannelMode::side));
        setBand (p, 12, FilterType::lowShelf, 250.0f, -3.0f, 0.71f, 3, true);
        set (p, "band12_channel", static_cast<float> (ChannelMode::mid));
    };

    ParametricEQAudioProcessor linear, minimum;
    setUp (linear);
    setUp (minimum);
    linear.setLinearPhase (true);
    linear.setLinearPhaseLength (length);
    prepare (linear, fs);
    prepare (minimum, fs);
    REQUIRE (settle (linear));

    for (int input = 0; input < 2; ++input)
    {
        const auto a = impulseResponse (linear, 16384 + 4096, input);
        const auto b = impulseResponse (minimum, 16384 + 4096, input);

        for (int output = 0; output < 2; ++output)
        {
            double worst = 0.0, worstAt = 0.0;
            for (int k = 0; k < 60; ++k)
            {
                const auto f = 200.0 * std::pow (18000.0 / 200.0, k / 59.0);
                const auto la = magnitudeDb (a, output, f, fs);
                const auto lb = magnitudeDb (b, output, f, fs);
                if (la < -60.0 && lb < -60.0)
                    continue;   // cross terms near zero
                if (std::abs (la - lb) > worst)
                {
                    worst = std::abs (la - lb);
                    worstAt = f;
                }
            }

            INFO ("input " << input << " -> output " << output << ": worst " << worst << " dB at " << worstAt << " Hz");
            CHECK (worst <= 0.1);
        }
    }
}

TEST_CASE ("In Linear phase mode, parameter changes reach the audio quickly and without clicks", "[linearphase][update]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double fs = 48000.0;
    ParametricEQAudioProcessor p;
    setBand (p, 4, FilterType::bell, 1000.0f, 0.0f, 1.0f, 3, true);
    p.setLinearPhase (true);   // 8192 taps
    prepare (p, fs);
    REQUIRE (settle (p));

    // A steady 1 kHz sine at -6 dBFS; cut the bell to -12 dB and time the change.
    juce::MidiBuffer midi;
    int sample = 0, changedAt = -1;
    bool finite = true;
    double largest = 0.0;
    float previous = 0.0f;
    const auto startMs = juce::Time::getMillisecondCounterHiRes();
    double reachedMs = -1.0;

    for (int block = 0; block < 400; ++block)
    {
        juce::AudioBuffer<float> b (2, 512);
        for (int i = 0; i < 512; ++i, ++sample)
            for (int ch = 0; ch < 2; ++ch)
                b.setSample (ch, i, 0.5f * static_cast<float> (std::sin (2.0 * std::numbers::pi * 1000.0 * sample / fs)));

        if (block == 20)
        {
            set (p, "band4_gain", -12.0f);
            changedAt = sample;
        }

        p.processBlock (b, midi);

        for (int i = 0; i < 512; ++i)
        {
            const auto v = b.getSample (0, i);
            finite = finite && std::isfinite (v);
            largest = std::max (largest, static_cast<double> (std::abs (v - previous)));
            previous = v;
        }

        if (changedAt >= 0 && reachedMs < 0.0 && b.getMagnitude (0, 0, 512) < 0.5f * std::pow (10.0f, -11.5f / 20.0f))
            reachedMs = juce::Time::getMillisecondCounterHiRes() - startMs;

        juce::Thread::sleep (1);   // real-time-like pacing so the designer thread can run
    }

    INFO ("reached -12 dB after " << reachedMs << " ms of wall-clock time");
    CHECK (finite);
    CHECK (largest < 0.2);
    CHECK (reachedMs > 0.0);
    CHECK (reachedMs < 1500.0);   // Debug build, polling every 20 ms plus design time
}

TEST_CASE ("Switching phase mode and length fades cleanly and ends in the right state", "[linearphase][switch]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double fs = 48000.0;
    ParametricEQAudioProcessor p;
    prepare (p, fs);

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
            juce::Thread::sleep (1);
        }
    };

    play (20);
    p.setLinearPhase (true);
    play (300);
    CHECK (p.isPhaseModeSettled());
    p.setLinearPhaseLength (2);
    play (400);
    CHECK (p.isPhaseModeSettled());
    p.setLinearPhase (false);
    play (40);
    CHECK (p.isPhaseModeSettled());

    CHECK (finite);
    CHECK (largest < 0.2);   // a 440 Hz sine at 0.5 steps at most 0.029 per sample
    CHECK (p.getLatencySamples() == 0);
}

TEST_CASE ("Dynamic bands in Linear phase mode act after the filter, as in Zero latency", "[linearphase][dynamics]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double fs = 48000.0;
    ParametricEQAudioProcessor p;
    setBand (p, 4, FilterType::bell, 1000.0f, 0.0f, 1.0f, 3, true);
    set (p, "band4_dyn", 1.0f);
    set (p, "band4_thresh", -30.0f);
    set (p, "band4_range", -6.0f);
    set (p, "band4_attack", 1.0f);
    setBand (p, 8, FilterType::bell, 5000.0f, 3.0f, 1.0f, 3, true);   // static: in the filter
    p.setLinearPhase (true);
    prepare (p, fs);
    REQUIRE (settle (p));

    juce::AudioBuffer<float> x (2, static_cast<int> (fs));
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < x.getNumSamples(); ++i)
            x.setSample (ch, i, static_cast<float> (std::pow (10.0, -10.0 / 20.0) * std::sin (2.0 * std::numbers::pi * 1000.0 * i / fs)));
    run (p, x);

    // -10 dBFS tone, full -6 dB range, plus the static 5 kHz bell's effect at 1 kHz.
    const auto staticBell = p.getBandSettings()[7];
    const auto bellAt1k = BandDesign::design (staticBell, fs).magnitudeDb (1000.0, fs);
    const auto level = juce::Decibels::gainToDecibels (static_cast<double> (x.getMagnitude (0, 38400, 9600)));
    INFO ("5 kHz bell at 1 kHz: " << bellAt1k << " dB");
    CHECK_THAT (level, WithinAbs (-16.0 + bellAt1k, 0.3));
    CHECK_THAT (p.getLiveGainChangeDb (4, 0), WithinAbs (-6.0f, 0.3f));
}

TEST_CASE ("Toggling dynamics in Linear phase mode stays clean", "[linearphase][dynamics]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double fs = 48000.0;
    ParametricEQAudioProcessor p;
    setBand (p, 4, FilterType::bell, 1000.0f, 4.0f, 1.0f, 3, true);
    p.setLinearPhase (true);
    prepare (p, fs);
    REQUIRE (settle (p));

    juce::MidiBuffer midi;
    bool finite = true;
    double largest = 0.0;
    float previous = 0.0f;
    int sample = 0;

    for (int block = 0; block < 200; ++block)
    {
        juce::AudioBuffer<float> b (2, 512);
        for (int i = 0; i < 512; ++i, ++sample)
            for (int ch = 0; ch < 2; ++ch)
                b.setSample (ch, i, 0.3f * static_cast<float> (std::sin (2.0 * std::numbers::pi * 1000.0 * sample / fs)));
        if (block % 50 == 10)
            set (p, "band4_dyn", block % 100 == 10 ? 1.0f : 0.0f);
        p.processBlock (b, midi);
        for (int i = 0; i < 512; ++i)
        {
            const auto v = b.getSample (0, i);
            finite = finite && std::isfinite (v);
            largest = std::max (largest, static_cast<double> (std::abs (v - previous)));
            previous = v;
        }
        juce::Thread::sleep (1);
    }

    CHECK (finite);
    CHECK (largest < 0.2);
}

TEST_CASE ("Longest linear-phase setting with Mid/Side bands at 96 kHz runs faster than real time", "[linearphase][cpu]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double fs = 96000.0;
    ParametricEQAudioProcessor p;
    for (int band = 1; band <= Parameters::numBands; ++band)
        setBand (p, band, FilterType::bell, static_cast<float> (40.0 * std::pow (2.0, (band - 1) * 0.6)), 2.0f, 1.0f, 3, true);
    set (p, "band5_channel", static_cast<float> (ChannelMode::side));   // four convolutions
    p.setLinearPhase (true);
    p.setLinearPhaseLength (2);
    prepare (p, fs);
    REQUIRE (settle (p));

    juce::AudioBuffer<float> x (2, static_cast<int> (fs));
    juce::Random random (8);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < x.getNumSamples(); ++i)
            x.setSample (ch, i, random.nextFloat() - 0.5f);

    const auto start = juce::Time::getMillisecondCounterHiRes();
    run (p, x);
    const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - start;

    bool finite = true;
    largestStep (x, finite);
    CHECK (finite);
    WARN ("32768-tap linear phase with Mid/Side at 96 kHz: 1 s of audio in " << elapsedMs << " ms ("
          << 1000.0 / elapsedMs << "x real time)");
    CHECK (elapsedMs < 1000.0);
}
