#include "dsp/SpectralDynamicsEngine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <numbers>

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double fs = 48000.0;
    using Engine = SpectralDynamicsEngine;

    std::array<BandSettings, 16> noBands()
    {
        std::array<BandSettings, 16> b;
        for (auto& s : b)
            s.inUse = false;
        return b;
    }

    BandSettings spectralBell (double f, double q, double thresholdDb, double rangeDb, ChannelMode mode = ChannelMode::stereo)
    {
        BandSettings s;
        s.type = FilterType::bell;
        s.frequencyHz = f;
        s.q = q;
        s.gainDb = 0.0;
        s.channel = mode;
        s.dynamics.on = true;
        s.dynamics.spectral = true;
        s.dynamics.thresholdDb = thresholdDb;
        s.dynamics.rangeDb = rangeDb;
        s.dynamics.attackMs = 1.0;
        s.dynamics.releaseMs = 50.0;
        return s;
    }

    /** Sum of sines per channel: {frequency, level dBFS} (-inf level: silent). */
    juce::AudioBuffer<float> tones (int numSamples, std::vector<std::pair<double, double>> left, std::vector<std::pair<double, double>> right)
    {
        juce::AudioBuffer<float> b (2, numSamples);
        b.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (auto [f, db] : (ch == 0 ? left : right))
                for (int i = 0; i < numSamples; ++i)
                    b.addSample (ch, i, static_cast<float> (std::pow (10.0, db / 20.0) * std::sin (2.0 * std::numbers::pi * f * i / fs)));
        return b;
    }

    void run (Engine& e, juce::AudioBuffer<float>& b, const juce::AudioBuffer<float>* side = nullptr)
    {
        for (int start = 0; start < b.getNumSamples(); start += 512)
        {
            const auto n = std::min (512, b.getNumSamples() - start);
            juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 2, start, n);
            if (side != nullptr)
            {
                juce::AudioBuffer<float> sideView (const_cast<float* const*> (side->getArrayOfReadPointers()), 2, start, n);
                e.process (view, &sideView);
            }
            else
            {
                e.process (view, nullptr);
            }
        }
    }

    /** Level (dBFS) of one frequency in the last `length` samples of a channel (sine amplitude, Hann-windowed DFT). */
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
TEST_CASE ("Without spectral bands the engine is an exact delay of its reported latency", "[spectral]")
{
    Engine e;
    e.prepare (fs);
    e.setBands (noBands());
    CHECK (Engine::latencySamples == Engine::fftSize);

    juce::AudioBuffer<float> x (2, 24000);
    juce::Random random (3);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < x.getNumSamples(); ++i)
            x.setSample (ch, i, random.nextFloat() - 0.5f);
    auto y = x;
    run (e, y);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = Engine::latencySamples; i < x.getNumSamples(); ++i)
            REQUIRE_THAT (y.getSample (ch, i), WithinAbs (x.getSample (ch, i - Engine::latencySamples), 1.0e-5f));
}

TEST_CASE ("A spectral band below its threshold changes nothing", "[spectral]")
{
    Engine e;
    e.prepare (fs);
    auto bands = noBands();
    bands[3] = spectralBell (2000.0, 1.0, -10.0, -12.0);
    e.setBands (bands);

    auto x = tones (24000, { { 2000.0, -40.0 }, { 2600.0, -45.0 } }, { { 2000.0, -40.0 } });
    auto y = x;
    run (e, y);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = Engine::latencySamples; i < x.getNumSamples(); ++i)
            REQUIRE_THAT (y.getSample (ch, i), WithinAbs (x.getSample (ch, i - Engine::latencySamples), 1.0e-5f));
}

TEST_CASE ("Each frequency slice is compressed on its own: a loud tone is cut, a quiet one beside it is not", "[spectral]")
{
    // A broadband dynamic band would cut both tones; the spectral band cuts only the one above threshold.
    Engine e;
    e.prepare (fs);
    auto bands = noBands();
    bands[3] = spectralBell (2200.0, 0.7, -40.0, -12.0);
    e.setBands (bands);

    auto y = tones (static_cast<int> (fs), { { 2000.0, -10.0 }, { 2800.0, -55.0 } }, { { 2000.0, -10.0 }, { 2800.0, -55.0 } });
    run (e, y);

    INFO ("2 kHz: " << toneDb (y, 0, 2000.0) << " dB, 2.8 kHz: " << toneDb (y, 0, 2800.0) << " dB");
    CHECK_THAT (toneDb (y, 0, 2000.0), WithinAbs (-10.0 - 12.0 * Engine::regionWeight (bands[3], 2000.0), 1.0));
    CHECK_THAT (toneDb (y, 0, 2800.0), WithinAbs (-55.0, 0.5));

    // The published slice gain at 2 kHz shows the cut.
    const auto bin = juce::roundToInt (2000.0 * Engine::fftSize / fs);
    CHECK_THAT (e.getSliceGainDb (3, bin), WithinAbs (-12.0 * Engine::regionWeight (bands[3], 2000.0), 1.0));
}

TEST_CASE ("Frequencies outside the band's region are untouched", "[spectral]")
{
    Engine e;
    e.prepare (fs);
    auto bands = noBands();
    bands[3] = spectralBell (4000.0, 2.0, -40.0, -12.0);
    e.setBands (bands);

    auto y = tones (static_cast<int> (fs), { { 200.0, -10.0 } }, { { 200.0, -10.0 } });
    run (e, y);
    CHECK_THAT (toneDb (y, 0, 200.0), WithinAbs (-10.0, 0.05));
    CHECK (Engine::regionWeight (bands[3], 200.0) < 0.001);
    CHECK_THAT (Engine::regionWeight (bands[3], 4000.0), WithinAbs (1.0, 1e-9));
}

TEST_CASE ("Attack and release set how fast slices are cut and recover", "[spectral]")
{
    auto halfCutTime = [] (double attackMs)
    {
        Engine e;
        e.prepare (fs);
        auto bands = noBands();
        bands[3] = spectralBell (2000.0, 1.0, -22.0, -12.0);   // 12 dB below the tone: the attack decides how fast
        bands[3].dynamics.attackMs = attackMs;
        e.setBands (bands);
        auto y = tones (static_cast<int> (fs), { { 2000.0, -10.0 } }, { { 2000.0, -10.0 } });
        run (e, y);
        // First 10 ms window (after the latency) that is at least 6 dB down.
        for (int w = 0; w < 90; ++w)
        {
            juce::AudioBuffer<float> part (2, 480);
            for (int ch = 0; ch < 2; ++ch)
                part.copyFrom (ch, 0, y, ch, Engine::latencySamples + w * 480, 480);
            if (juce::Decibels::gainToDecibels (part.getMagnitude (0, 0, 480)) < -16.0)
                return w * 10;
        }
        return 1000;
    };

    const auto fast = halfCutTime (1.0), slow = halfCutTime (150.0);
    INFO ("fast " << fast << " ms, slow " << slow << " ms");
    CHECK (fast < slow);
    CHECK (fast <= 60);

    // Release: loud, then quiet below threshold: back to the input level.
    Engine e;
    e.prepare (fs);
    auto bands = noBands();
    bands[3] = spectralBell (2000.0, 1.0, -40.0, -12.0);
    e.setBands (bands);
    auto loud = tones (24000, { { 2000.0, -10.0 } }, { { 2000.0, -10.0 } });
    run (e, loud);
    auto quiet = tones (48000, { { 2000.0, -60.0 } }, { { 2000.0, -60.0 } });
    run (e, quiet);
    CHECK_THAT (toneDb (quiet, 0, 2000.0), WithinAbs (-60.0, 0.3));
}

TEST_CASE ("With side-chain, each side-chain slice ducks only its own frequencies", "[spectral][sidechain]")
{
    Engine e;
    e.prepare (fs);
    auto bands = noBands();
    bands[3] = spectralBell (2500.0, 0.5, -30.0, -12.0);
    bands[3].dynamics.sidechain = true;
    e.setBands (bands);

    auto main = tones (static_cast<int> (fs), { { 2000.0, -40.0 }, { 3000.0, -40.0 } }, { { 2000.0, -40.0 }, { 3000.0, -40.0 } });
    const auto side = tones (static_cast<int> (fs), { { 2000.0, -6.0 } }, { { 2000.0, -6.0 } });
    run (e, main, &side);

    INFO ("2 kHz " << toneDb (main, 0, 2000.0) << " dB, 3 kHz " << toneDb (main, 0, 3000.0) << " dB");
    CHECK (toneDb (main, 0, 2000.0) < -46.0);
    CHECK_THAT (toneDb (main, 0, 3000.0), WithinAbs (-40.0, 0.5));
}

TEST_CASE ("Stereo spectral bands act per channel; Side bands only on side content", "[spectral]")
{
    SECTION ("loud left, quiet right")
    {
        Engine e;
        e.prepare (fs);
        auto bands = noBands();
        bands[3] = spectralBell (2000.0, 1.0, -40.0, -12.0);
        e.setBands (bands);
        auto y = tones (static_cast<int> (fs), { { 2000.0, -10.0 } }, { { 2000.0, -50.0 } });
        run (e, y);
        CHECK (toneDb (y, 0, 2000.0) < -20.0);
        CHECK_THAT (toneDb (y, 1, 2000.0), WithinAbs (-50.0, 0.3));
    }

    SECTION ("Side band: mid content untouched, side content cut")
    {
        for (bool sideContent : { false, true })
        {
            Engine e;
            e.prepare (fs);
            auto bands = noBands();
            bands[3] = spectralBell (2000.0, 1.0, -40.0, -12.0, ChannelMode::side);
            e.setBands (bands);
            auto y = tones (static_cast<int> (fs), { { 2000.0, -10.0 } }, { { 2000.0, -10.0 } });
            if (sideContent)
                y.applyGain (1, 0, y.getNumSamples(), -1.0f);
            run (e, y);
            INFO ((sideContent ? "side" : "mid") << " content: " << toneDb (y, 0, 2000.0) << " dB");
            if (sideContent)
                CHECK (toneDb (y, 0, 2000.0) < -20.0);
            else
                CHECK_THAT (toneDb (y, 0, 2000.0), WithinAbs (-10.0, 0.3));
        }
    }
}

TEST_CASE ("Only active dynamic bands with Spectral on take part", "[spectral]")
{
    auto bands = noBands();
    bands[1] = spectralBell (2000.0, 1.0, -40.0, -12.0);
    bands[1].dynamics.spectral = false;            // dynamic but not spectral
    bands[2] = spectralBell (2000.0, 1.0, -40.0, -12.0);
    bands[2].enabled = false;                      // disabled
    bands[3] = spectralBell (2000.0, 1.0, -40.0, -12.0);
    bands[3].type = FilterType::notch;             // cannot be dynamic
    CHECK_FALSE (bands[1].isSpectral());
    CHECK_FALSE (bands[2].isSpectral());
    CHECK_FALSE (bands[3].isSpectral());
    CHECK_FALSE (Engine::anySpectral (bands));

    bands[4] = spectralBell (2000.0, 1.0, -40.0, -12.0);
    CHECK (bands[4].isSpectral());
    CHECK (Engine::anySpectral (bands));
}

TEST_CASE ("16 spectral bands at 96 kHz run faster than real time", "[spectral][cpu]")
{
    constexpr double rate = 96000.0;
    Engine e;
    e.prepare (rate);
    auto bands = noBands();
    for (int b = 0; b < 16; ++b)
        bands[static_cast<size_t> (b)] = spectralBell (40.0 * std::pow (2.0, b * 0.6), 1.0, -40.0, -12.0);
    e.setBands (bands);

    juce::AudioBuffer<float> x (2, static_cast<int> (rate));
    juce::Random random (8);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < x.getNumSamples(); ++i)
            x.setSample (ch, i, random.nextFloat() - 0.5f);

    const auto start = juce::Time::getMillisecondCounterHiRes();
    run (e, x);
    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - start;
    WARN ("16 spectral bands at 96 kHz: 1 s of audio in " << elapsed << " ms");
    CHECK (elapsed < 1000.0);
}
