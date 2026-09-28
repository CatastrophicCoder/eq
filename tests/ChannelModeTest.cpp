#include "TestParameters.h"

#include "PluginEditor.h"
#include "dsp/AutoGain.h"
#include "dsp/BandDesign.h"
#include "dsp/CascadeProcessor.h"
#include "dsp/KWeighting.h"
#include "dsp/StereoTransfer.h"
#include "ui/ResponseCurves.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <numbers>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace TestParameters;
using Complex = std::complex<double>;

namespace
{
    void setChannel (ParametricEQAudioProcessor& p, int band, ChannelMode mode)
    {
        set (p, Parameters::id (band, "channel"), static_cast<float> (mode));
    }

    void prepare (ParametricEQAudioProcessor& p, double fs = 48000.0)
    {
        p.setPlayConfigDetails (2, 2, fs, 512);
        p.prepareToPlay (fs, 512);
    }

    void processInBlocks (ParametricEQAudioProcessor& p, juce::AudioBuffer<float>& signal)
    {
        juce::MidiBuffer midi;
        for (int start = 0; start < signal.getNumSamples(); start += 512)
        {
            const auto n = std::min (512, signal.getNumSamples() - start);
            juce::AudioBuffer<float> view (signal.getArrayOfWritePointers(), 2, start, n);
            p.processBlock (view, midi);
        }
    }

    juce::AudioBuffer<float> stereoNoise (int numSamples, int seed, float leftScale = 1.0f, float rightScale = 1.0f,
                                          bool correlated = false)
    {
        juce::AudioBuffer<float> b (2, numSamples);
        juce::Random random (seed);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto a = random.nextFloat() - 0.5f;
            const auto c = correlated ? a : random.nextFloat() - 0.5f;
            b.setSample (0, i, leftScale * a);
            b.setSample (1, i, rightScale * c);
        }
        return b;
    }

    BandSettings band (FilterType type, double f, double gain, double q, ChannelMode mode, int slope = 3)
    {
        BandSettings s;
        s.type = type; s.frequencyHz = f; s.gainDb = gain; s.q = q; s.slopeIndex = slope; s.channel = mode;
        return s;
    }

    std::array<BandSettings, 16> freeBands()
    {
        std::array<BandSettings, 16> b;
        for (auto& s : b) { s.enabled = false; s.inUse = false; }
        return b;
    }

    bool near (const StereoTransfer::Matrix& a, const StereoTransfer::Matrix& b, double tol = 1e-12)
    {
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 2; ++c)
                if (std::abs (a[static_cast<size_t> (r)][static_cast<size_t> (c)] - b[static_cast<size_t> (r)][static_cast<size_t> (c)]) > tol)
                    return false;
        return true;
    }

    double dftDb (const float* x, int n, double f, double fs)
    {
        const auto step = std::polar (1.0, -2.0 * std::numbers::pi * f / fs);
        Complex phasor { 1.0, 0.0 }, sum {};
        for (int i = 0; i < n; ++i)
        {
            sum += static_cast<double> (x[i]) * phasor;
            phasor *= step;
            if ((i & 1023) == 1023)
                phasor /= std::abs (phasor);
        }
        return 20.0 * std::log10 (std::max (1e-15, std::abs (sum)));
    }
}

//==============================================================================
TEST_CASE ("Channel modes have a fixed order and names", "[channel]")
{
    CHECK (static_cast<int> (ChannelMode::stereo) == 0);
    CHECK (static_cast<int> (ChannelMode::left) == 1);
    CHECK (static_cast<int> (ChannelMode::right) == 2);
    CHECK (static_cast<int> (ChannelMode::mid) == 3);
    CHECK (static_cast<int> (ChannelMode::side) == 4);
    CHECK (ChannelModes::count == 5);
    CHECK (juce::String (ChannelModes::names[3]) == "Mid");
    CHECK (juce::String (ChannelModes::letters[0]).isEmpty());
    CHECK (juce::String (ChannelModes::letters[4]) == "S");
}

TEST_CASE ("Every band has a channel parameter: Stereo by default, hint 3", "[channel][parameters]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    for (int b = 1; b <= Parameters::numBands; ++b)
    {
        INFO ("band " << b);
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&param (p, Parameters::id (b, "channel")));
        REQUIRE (choice != nullptr);
        REQUIRE (choice->choices.size() == ChannelModes::count);
        for (int i = 0; i < ChannelModes::count; ++i)
            CHECK (choice->choices[i] == ChannelModes::names[i]);
        CHECK (choice->getIndex() == static_cast<int> (ChannelMode::stereo));
        CHECK (choice->getVersionHint() == 3);
    }

    CHECK (p.getBandSettings()[4].channel == ChannelMode::stereo);
    setChannel (p, 5, ChannelMode::side);
    CHECK (p.getBandSettings()[4].channel == ChannelMode::side);
}

TEST_CASE ("A session without channel parameters loads every band as Stereo", "[channel][state]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    juce::XmlElement v3 ("ParametricEQ");
    v3.setAttribute ("stateVersion", 3);
    v3.setAttribute ("band2_used", 1);
    auto* child = v3.createNewChildElement ("PARAM");
    child->setAttribute ("id", "band2_enabled");
    child->setAttribute ("value", 1.0);

    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (v3, block);

    ParametricEQAudioProcessor p;
    setChannel (p, 2, ChannelMode::mid);   // must be reset by the load
    p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));

    for (int b = 1; b <= Parameters::numBands; ++b)
        CHECK (p.getBandSettings()[static_cast<size_t> (b - 1)].channel == ChannelMode::stereo);
}

//==============================================================================
TEST_CASE ("Complex responses agree with the magnitudes and phases", "[channel][transfer]")
{
    const BiquadCoefficients delay { 0.0, 1.0, 0.0, 0.0, 0.0 };   // z^-1
    const auto w = 2.0 * std::numbers::pi * 1000.0 / 48000.0;
    CHECK (std::abs (delay.response (1000.0, 48000.0) - std::polar (1.0, -w)) < 1e-12);

    const auto bell = BandDesign::design (band (FilterType::bell, 1000.0, 6.0, 1.0, ChannelMode::stereo), 48000.0);
    for (auto f : { 50.0, 1000.0, 9000.0 })
        CHECK_THAT (20.0 * std::log10 (std::abs (bell.response (f, 48000.0))), WithinAbs (bell.magnitudeDb (f, 48000.0), 1e-9));

    const auto cut = BandDesign::design (band (FilterType::highCut, 2000.0, 0.0, 0.71, ChannelMode::stereo, 7), 48000.0);
    CHECK_THAT (20.0 * std::log10 (std::abs (cut.response (4000.0, 48000.0))), WithinAbs (cut.magnitudeDb (4000.0, 48000.0), 1e-9));
}

TEST_CASE ("Band matrices for each channel mode", "[channel][transfer]")
{
    const Complex h { 0.3, -1.2 };
    using M = StereoTransfer::Matrix;
    const Complex one { 1.0, 0.0 }, zero {};

    CHECK (near (StereoTransfer::identity(), M { { { one, zero }, { zero, one } } }));
    CHECK (near (StereoTransfer::forBand (ChannelMode::stereo, h), M { { { h, zero }, { zero, h } } }));
    CHECK (near (StereoTransfer::forBand (ChannelMode::left, h), M { { { h, zero }, { zero, one } } }));
    CHECK (near (StereoTransfer::forBand (ChannelMode::right, h), M { { { one, zero }, { zero, h } } }));

    // Mid: T^-1 diag (H, 1) T = [[(H+1)/2, (H-1)/2], [(H-1)/2, (H+1)/2]]; Side swaps the roles.
    const auto p = (h + one) / 2.0, m = (h - one) / 2.0;
    CHECK (near (StereoTransfer::forBand (ChannelMode::mid, h), M { { { p, m }, { m, p } } }));
    CHECK (near (StereoTransfer::forBand (ChannelMode::side, h), M { { { p, -m }, { -m, p } } }));

    // In the M/S basis, a Mid band is diag (H, 1).
    CHECK (near (StereoTransfer::toMidSide (StereoTransfer::forBand (ChannelMode::mid, h)), M { { { h, zero }, { zero, one } } }));
}

TEST_CASE ("Power gain and chain order", "[channel][transfer]")
{
    CHECK_THAT (StereoTransfer::powerGain (StereoTransfer::identity()), WithinAbs (1.0, 1e-12));

    // A flat gain g on one channel (or on mid, or side) keeps half the power unchanged: (g^2 + 1) / 2.
    const Complex g { 2.0, 0.0 };
    for (auto mode : { ChannelMode::left, ChannelMode::right, ChannelMode::mid, ChannelMode::side })
        CHECK_THAT (StereoTransfer::powerGain (StereoTransfer::forBand (mode, g)), WithinAbs (2.5, 1e-12));
    CHECK_THAT (StereoTransfer::powerGain (StereoTransfer::forBand (ChannelMode::stereo, g)), WithinAbs (4.0, 1e-12));

    // Left and Mid bands do not commute: the chain follows band order.
    auto bands = freeBands();
    bands[0] = band (FilterType::bell, 1000.0, 9.0, 1.0, ChannelMode::left);
    bands[1] = band (FilterType::bell, 1000.0, -6.0, 2.0, ChannelMode::mid);
    const auto chain = StereoTransfer::chain (bands, 1000.0, 48000.0, true);

    const auto h0 = BandDesign::design (bands[0], 48000.0).response (1000.0, 48000.0);
    const auto h1 = BandDesign::design (bands[1], 48000.0).response (1000.0, 48000.0);
    const auto expected = StereoTransfer::multiply (StereoTransfer::forBand (ChannelMode::mid, h1),
                                                    StereoTransfer::forBand (ChannelMode::left, h0));
    CHECK (near (chain, expected, 1e-12));
    CHECK_FALSE (near (chain, StereoTransfer::multiply (StereoTransfer::forBand (ChannelMode::left, h0),
                                                        StereoTransfer::forBand (ChannelMode::mid, h1)), 1e-6));

    // Cuts are left out on request; inactive bands always.
    bands[2] = band (FilterType::highCut, 500.0, 0.0, 0.71, ChannelMode::stereo);
    bands[3] = band (FilterType::bell, 1000.0, 12.0, 1.0, ChannelMode::stereo);
    bands[3].enabled = false;
    CHECK (near (StereoTransfer::chain (bands, 1000.0, 48000.0, false), chain, 1e-12));
}

//==============================================================================
TEST_CASE ("A Left band filters only the left channel; Right only the right", "[channel][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    for (auto mode : { ChannelMode::left, ChannelMode::right })
    {
        ParametricEQAudioProcessor stereo, oneSide;
        for (auto* p : { &stereo, &oneSide })
            setBand (*p, 3, FilterType::bell, 1000.0f, 9.0f, 1.5f, 3, true);
        setChannel (oneSide, 3, mode);
        prepare (stereo);
        prepare (oneSide);

        auto input = stereoNoise (8192, 4);
        auto a = input, b = input;
        a.makeCopyOf (input);
        b.makeCopyOf (input);
        processInBlocks (stereo, a);
        processInBlocks (oneSide, b);

        const auto filtered = mode == ChannelMode::left ? 0 : 1;
        const auto untouched = 1 - filtered;
        INFO ((mode == ChannelMode::left ? "left" : "right"));
        for (int i = 0; i < 8192; ++i)
        {
            REQUIRE (juce::exactlyEqual (b.getSample (untouched, i), input.getSample (untouched, i)));
            REQUIRE (juce::exactlyEqual (b.getSample (filtered, i), a.getSample (filtered, i)));
        }
    }
}

TEST_CASE ("A Mid band filters the mid signal and leaves the side signal alone; Side the opposite", "[channel][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    for (auto mode : { ChannelMode::mid, ChannelMode::side })
    {
        // A fresh processor per signal: the processor has no reset() that clears its filters.
        ParametricEQAudioProcessor stereo, ms, msOther;
        for (auto* p : { &stereo, &ms, &msOther })
            setBand (*p, 3, FilterType::bell, 1000.0f, 9.0f, 1.5f, 3, true);
        setChannel (ms, 3, mode);
        setChannel (msOther, 3, mode);
        prepare (stereo);
        prepare (ms);
        prepare (msOther);

        // The part the band acts on: mid (L = R) for Mid, side (L = -R) for Side.
        const auto actsOn = stereoNoise (8192, 5, 1.0f, mode == ChannelMode::mid ? 1.0f : -1.0f, true);
        const auto other  = stereoNoise (8192, 6, 1.0f, mode == ChannelMode::mid ? -1.0f : 1.0f, true);

        juce::AudioBuffer<float> refA, msA, msB;
        refA.makeCopyOf (actsOn);
        msA.makeCopyOf (actsOn);
        msB.makeCopyOf (other);
        processInBlocks (stereo, refA);
        processInBlocks (ms, msA);
        processInBlocks (msOther, msB);

        INFO ((mode == ChannelMode::mid ? "mid" : "side"));
        for (int i = 0; i < 8192; ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                // Filtered exactly like a Stereo band would filter it ...
                REQUIRE_THAT (msA.getSample (ch, i), WithinAbs (refA.getSample (ch, i), 1e-6f));
                // ... and the other part passes through untouched.
                REQUIRE (juce::exactlyEqual (msB.getSample (ch, i), other.getSample (ch, i)));
            }
    }
}

TEST_CASE ("Mid/Side encoding and decoding returns the input with a neutral filter", "[channel][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    for (auto mode : { ChannelMode::mid, ChannelMode::side })
    {
        ParametricEQAudioProcessor p;
        setBand (p, 7, FilterType::bell, 1000.0f, 0.0f, 1.0f, 3, true);
        setChannel (p, 7, mode);
        prepare (p);

        auto input = stereoNoise (4096, 7);
        juce::AudioBuffer<float> out;
        out.makeCopyOf (input);
        processInBlocks (p, out);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4096; ++i)
                REQUIRE_THAT (out.getSample (ch, i), WithinAbs (input.getSample (ch, i), 1e-6f));
    }
}

TEST_CASE ("Changing a band's channel mode while playing crossfades", "[channel][dsp]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double fs = 44100.0;
    ParametricEQAudioProcessor p;
    setBand (p, 4, FilterType::bell, 1000.0f, 12.0f, 1.0f, 3, true);
    prepare (p, fs);

    juce::AudioBuffer<float> buffer (2, 16384);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 16384; ++i)
            buffer.setSample (ch, i, static_cast<float> (0.25 * std::sin (2.0 * std::numbers::pi * 1000.0 * i / fs)));

    juce::MidiBuffer midi;
    const ChannelMode sequence[] { ChannelMode::left, ChannelMode::mid, ChannelMode::side, ChannelMode::right, ChannelMode::stereo };
    for (int start = 0, step = 0; start < 16384; start += 512)
    {
        if (start % 3072 == 2048 && step < 5)
            setChannel (p, 4, sequence[step++]);
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, start, 512);
        p.processBlock (view, midi);
    }

    double largest = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 1; i < 16384; ++i)
            largest = std::max (largest, std::abs (static_cast<double> (buffer.getSample (ch, i)) - buffer.getSample (ch, i - 1)));
    CHECK (largest < 0.2);
}

//==============================================================================
TEST_CASE ("Auto Gain uses the 2x2 model; all-Stereo chains are unchanged", "[channel][autogain]")
{
    auto bands = freeBands();
    bands[0] = band (FilterType::bell, 1000.0, 6.0, 1.0, ChannelMode::stereo);
    bands[1] = band (FilterType::lowShelf, 120.0, -4.0, 0.71, ChannelMode::stereo);
    const auto stereoOffset = AutoGain::computeOffsetDb (bands, 48000.0);

    // The same K-weighted pink average with plain |H|^2 (the pre-M6 formula).
    double num = 0.0, den = 0.0;
    for (int k = 0; k < AutoGain::numPoints; ++k)
    {
        const auto f = 20.0 * std::pow (1000.0, k / (AutoGain::numPoints - 1.0));
        const auto w = std::pow (10.0, KWeighting::magnitudeDb (f) / 10.0);
        const auto db = BandDesign::design (bands[0], 48000.0).magnitudeDb (f, 48000.0)
                      + BandDesign::design (bands[1], 48000.0).magnitudeDb (f, 48000.0);
        num += w * std::pow (10.0, db / 10.0);
        den += w;
    }
    CHECK_THAT (stereoOffset, WithinAbs (-10.0 * std::log10 (num / den), 1e-6));

    // A band on one channel counts for half the power.
    auto left = freeBands();
    left[0] = band (FilterType::lowShelf, 20000.0, 12.0, 0.71, ChannelMode::left);   // ~+12 dB almost everywhere
    auto both = left;
    both[0].channel = ChannelMode::stereo;
    CHECK (AutoGain::computeOffsetDb (left, 48000.0) > AutoGain::computeOffsetDb (both, 48000.0) + 3.0);
}

TEST_CASE ("With Auto Gain on, K-weighted loudness of uncorrelated pink noise is unchanged across channel modes", "[channel][autogain]")
{
    // The model's own assumption, measured on real audio: pink noise with independent L and R,
    // K-weighted with the BS.1770 filters (their coefficients are for 48 kHz, as here).
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double fs = 48000.0;
    constexpr int order = 18, n = 1 << order;

    // Pink noise by spectral shaping: random phase, |X|^2 ~ 1/f, inverse FFT.
    auto pink = [&] (int seed)
    {
        juce::dsp::FFT fft (order);
        std::vector<Complex> spectrum (static_cast<size_t> (n)), time (static_cast<size_t> (n));
        juce::Random random (seed);
        for (int k = 1; k < n / 2; ++k)
        {
            const auto f = k * fs / n;
            const auto amplitude = (f >= 20.0 && f <= 20000.0) ? 1.0 / std::sqrt (f) : 0.0;
            const auto x = std::polar (amplitude, 2.0 * std::numbers::pi * random.nextDouble());
            spectrum[static_cast<size_t> (k)] = x;
            spectrum[static_cast<size_t> (n - k)] = std::conj (x);
        }
        std::vector<std::complex<float>> in (static_cast<size_t> (n)), out (static_cast<size_t> (n));
        for (int i = 0; i < n; ++i)
            in[static_cast<size_t> (i)] = std::complex<float> (spectrum[static_cast<size_t> (i)]);
        fft.perform (in.data(), out.data(), true);
        std::vector<float> result (static_cast<size_t> (n));
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = std::max (peak, std::abs (result[static_cast<size_t> (i)] = out[static_cast<size_t> (i)].real()));
        for (auto& v : result)
            v *= 0.25f / peak;
        return result;
    };

    const auto left = pink (1), right = pink (2);

    auto kLoudness = [&] (const juce::AudioBuffer<float>& b)
    {
        SectionCascade k;
        k.add ({ 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 });
        k.add ({ 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 });
        double power = 0.0;
        for (int ch = 0; ch < 2; ++ch)
        {
            CascadeProcessor filter;
            filter.setCoefficients (k);
            filter.reset();
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto y = filter.processSample (0, b.getSample (ch, i));
                if (i >= 4096)   // skip the start-up
                    power += y * y;
            }
        }
        return 10.0 * std::log10 (power);
    };

    ParametricEQAudioProcessor p;
    setBand (p, 2, FilterType::bell, 300.0f, 8.0f, 1.0f, 3, true);
    setChannel (p, 2, ChannelMode::left);
    setBand (p, 5, FilterType::highShelf, 4000.0f, 6.0f, 0.71f, 3, true);
    setChannel (p, 5, ChannelMode::side);
    setBand (p, 8, FilterType::bell, 1500.0f, -5.0f, 2.0f, 3, true);
    setChannel (p, 8, ChannelMode::mid);
    setBand (p, 11, FilterType::lowShelf, 150.0f, 4.0f, 0.71f, 3, true);   // stereo
    set (p, Parameters::autoGain, 1.0f);
    prepare (p, fs);

    const auto expected = static_cast<float> (AutoGain::computeOffsetDb (p.getBandSettings(), fs));
    for (int i = 0; i < 200 && std::abs (p.getAutoGainOffsetDb() - expected) > 1e-5f; ++i)
        juce::Thread::sleep (10);
    REQUIRE_THAT (p.getAutoGainOffsetDb(), WithinAbs (expected, 1e-4f));

    juce::AudioBuffer<float> input (2, n);
    for (int i = 0; i < n; ++i)
    {
        input.setSample (0, i, left[static_cast<size_t> (i)]);
        input.setSample (1, i, right[static_cast<size_t> (i)]);
    }
    juce::AudioBuffer<float> output;
    output.makeCopyOf (input);

    // Settle the output gain on the offset, then measure.
    juce::AudioBuffer<float> settle (2, 4800);
    settle.clear();
    processInBlocks (p, settle);
    processInBlocks (p, output);

    INFO ("offset " << expected << " dB");
    CHECK_THAT (kLoudness (output) - kLoudness (input), WithinAbs (0.0, 0.3));
}

//==============================================================================
TEST_CASE ("Sum curves: one for Stereo, L and R with L/R bands, M and S with M/S bands", "[channel][curves]")
{
    constexpr double fs = 48000.0;
    auto bands = freeBands();
    bands[0] = band (FilterType::bell, 1000.0, 6.0, 1.0, ChannelMode::stereo);

    ResponseCurves curves;
    curves.update (bands, fs);
    CHECK (curves.getSumLayout() == ResponseCurves::SumLayout::single);

    auto designDb = [&] (const BandSettings& b, double f) { return BandDesign::design (b, fs).magnitudeDb (f, fs); };

    // L/R: L = stereo + left, R = stereo only (exact).
    bands[1] = band (FilterType::bell, 3000.0, -8.0, 2.0, ChannelMode::left);
    curves.update (bands, fs);
    REQUIRE (curves.getSumLayout() == ResponseCurves::SumLayout::leftRight);
    for (int k = 0; k < ResponseCurves::numPoints; k += 13)
    {
        const auto f = curves.frequency (k);
        CHECK_THAT (curves.sumDb (k), WithinAbs (designDb (bands[0], f) + designDb (bands[1], f), 1e-6));
        CHECK_THAT (curves.secondSumDb (k), WithinAbs (designDb (bands[0], f), 1e-6));
    }

    // M/S only: M = stereo + mid, S = stereo + side (exact in the M/S basis).
    bands[1].channel = ChannelMode::mid;
    bands[2] = band (FilterType::highShelf, 6000.0, 4.0, 0.71, ChannelMode::side);
    curves.update (bands, fs);
    REQUIRE (curves.getSumLayout() == ResponseCurves::SumLayout::midSide);
    for (int k = 0; k < ResponseCurves::numPoints; k += 13)
    {
        const auto f = curves.frequency (k);
        CHECK_THAT (curves.sumDb (k), WithinAbs (designDb (bands[0], f) + designDb (bands[1], f), 1e-6));
        CHECK_THAT (curves.secondSumDb (k), WithinAbs (designDb (bands[0], f) + designDb (bands[2], f), 1e-6));
    }

    // Mixed: L and R from the chain matrix diagonal.
    bands[3] = band (FilterType::bell, 200.0, 5.0, 1.0, ChannelMode::right);
    curves.update (bands, fs);
    REQUIRE (curves.getSumLayout() == ResponseCurves::SumLayout::leftRight);
    for (int k = 0; k < ResponseCurves::numPoints; k += 13)
    {
        const auto m = StereoTransfer::chain (bands, curves.frequency (k), fs, true);
        CHECK_THAT (curves.sumDb (k), WithinAbs (20.0 * std::log10 (std::abs (m[0][0])), 1e-6));
        CHECK_THAT (curves.secondSumDb (k), WithinAbs (20.0 * std::log10 (std::abs (m[1][1])), 1e-6));
    }
}

TEST_CASE ("Displayed L and R sums match the measured left and right responses", "[channel][curves]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double fs = 48000.0;
    ParametricEQAudioProcessor p;
    setBand (p, 1, FilterType::bell, 400.0f, 6.0f, 1.0f, 3, true);
    setBand (p, 2, FilterType::bell, 3000.0f, -8.0f, 2.0f, 3, true);
    setChannel (p, 2, ChannelMode::left);
    setBand (p, 3, FilterType::highShelf, 8000.0f, 5.0f, 0.71f, 3, true);
    setChannel (p, 3, ChannelMode::right);
    prepare (p, fs);

    constexpr int n = 1 << 16;
    juce::AudioBuffer<float> ir (2, n);
    ir.clear();
    ir.setSample (0, 0, 1.0f);
    ir.setSample (1, 0, 1.0f);
    processInBlocks (p, ir);

    ResponseCurves curves;
    curves.update (p.getBandSettings(), fs);
    REQUIRE (curves.getSumLayout() == ResponseCurves::SumLayout::leftRight);

    for (int k = 0; k < ResponseCurves::numPoints; k += 32)
    {
        const auto f = curves.frequency (k);
        INFO ("f " << f);
        CHECK_THAT (dftDb (ir.getReadPointer (0), n, f, fs), WithinAbs (curves.sumDb (k), 0.1));
        CHECK_THAT (dftDb (ir.getReadPointer (1), n, f, fs), WithinAbs (curves.secondSumDb (k), 0.1));
    }
}

//==============================================================================
TEST_CASE ("The band panel and the node menu set the channel mode", "[channel][editor]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    setBand (p, 2, FilterType::bell, 400.0f, 3.0f, 1.0f, 3, true);
    setBand (p, 3, FilterType::bell, 900.0f, 3.0f, 1.0f, 3, true);
    setBand (p, 4, FilterType::bell, 3000.0f, 3.0f, 1.0f, 3, false);   // disabled
    std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
    auto& editor = *dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());
    auto& display = editor.getDisplay();

    // Panel.
    display.setSelection ({ 2 }, 2);
    auto& box = editor.getBandPanel().getChannelBox();
    REQUIRE (box.getNumItems() == ChannelModes::count);
    box.setSelectedItemIndex (static_cast<int> (ChannelMode::mid), juce::sendNotificationSync);
    CHECK (p.getBandSettings()[1].channel == ChannelMode::mid);

    // Menu: a Channel submenu, ticked on the current mode, applied to the selection, skipping disabled bands.
    const auto menu = display.buildNodeMenu (2);
    bool foundTicked = false;
    for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
        if (it.getItem().itemID == ResponseDisplay::menuChannelBase + static_cast<int> (ChannelMode::mid))
            foundTicked = it.getItem().isTicked;
    CHECK (foundTicked);

    display.setSelection ({ 2, 3, 4 }, 2);
    display.applyNodeMenuResult (3, ResponseDisplay::menuChannelBase + static_cast<int> (ChannelMode::side));
    CHECK (p.getBandSettings()[1].channel == ChannelMode::side);
    CHECK (p.getBandSettings()[2].channel == ChannelMode::side);
    CHECK (p.getBandSettings()[3].channel == ChannelMode::stereo);   // disabled: unchanged

    // Nodes carry their channel for the badge.
    editor.refreshControls();
    for (const auto& node : display.getNodes())
        if (node.band == 3)
            CHECK (node.channel == ChannelMode::side);
}
