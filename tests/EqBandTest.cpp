#include "dsp/BandDesign.h"
#include "dsp/CutSlope.h"
#include "dsp/EqBand.h"
#include "dsp/MatchedPeakingDesign.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double toleranceDb = 0.1;

    // Largest sample-to-sample step in the modulation tests (same as M1): a clean 1 kHz sine at
    // the highest output these tests produce (0.25 input, +12 dB = 1.0) steps by at most
    // 2 pi 1000 / 44100 = 0.142 per sample; 0.2 leaves about 40 % headroom.
    constexpr double maxStep = 0.2;

    BandSettings make (FilterType type, double f, double gain = 9.0, double q = 2.0, int slope = 3, bool enabled = true)
    {
        BandSettings s;
        s.type = type;
        s.frequencyHz = f;
        s.gainDb = gain;
        s.q = q;
        s.slopeIndex = slope;
        s.enabled = enabled;
        return s;
    }

    /** DFT of x at exactly frequencyHz, in dB (phasor recurrence, double). */
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

    void fillSine (juce::AudioBuffer<float>& buffer, double frequencyHz, double amplitude, double sampleRate, int startSample)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, static_cast<float> (amplitude * std::sin (2.0 * std::numbers::pi * frequencyHz
                                                                                   * (startSample + i) / sampleRate)));
    }

    struct Signal
    {
        bool allFinite = true;
        double largestStep = 0.0;
        float previous[2] {};
        bool hasPrevious = false;

        void add (const juce::AudioBuffer<float>& buffer)
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    const auto x = buffer.getSample (ch, i);
                    allFinite = allFinite && std::isfinite (x);

                    if (hasPrevious)
                        largestStep = std::max (largestStep, std::abs (static_cast<double> (x) - previous[ch]));

                    previous[ch] = x;
                }

                hasPrevious = true;
            }
        }
    };

    /** Runs a 1 kHz sine at 0.25 through the band in 64-sample blocks, calling
        change (sampleIndex) before each block. */
    template <typename Change>
    Signal run (EqBand& band, double sampleRate, int totalSamples, Change&& change)
    {
        constexpr int blockSize = 64;
        juce::AudioBuffer<float> block (2, blockSize);
        Signal signal;

        for (int start = 0; start < totalSamples; start += blockSize)
        {
            change (start);
            fillSine (block, 1000.0, 0.25, sampleRate, start);
            band.process (block);
            signal.add (block);
        }

        return signal;
    }

    bool sameCascade (const SectionCascade& a, const SectionCascade& b)
    {
        if (a.numSections != b.numSections)
            return false;

        for (int i = 0; i < a.numSections; ++i)
        {
            const auto& x = a.sections[static_cast<size_t> (i)];
            const auto& y = b.sections[static_cast<size_t> (i)];

            for (auto [p, r] : { std::pair { x.b0, y.b0 }, { x.b1, y.b1 }, { x.b2, y.b2 }, { x.a1, y.a1 }, { x.a2, y.a2 } })
                if (std::abs (p - r) > 1e-9)
                    return false;
        }

        return true;
    }

    const std::vector<FilterType> allTypes { FilterType::bell, FilterType::lowShelf, FilterType::highShelf,
                                             FilterType::lowCut, FilterType::highCut, FilterType::notch,
                                             FilterType::bandPass, FilterType::tiltShelf, FilterType::flatTilt,
                                             FilterType::allPass };
}

//==============================================================================
TEST_CASE ("EqBand loads the target coefficients on prepare", "[eqband]")
{
    EqBand band;
    const auto settings = make (FilterType::bell, 2500.0, -7.5, 2.0);
    band.setTargets (settings);
    band.prepare (48000.0, 2);

    CHECK (sameCascade (band.getActiveCascade(), BandDesign::design (settings, 48000.0)));
    CHECK_FALSE (band.isCrossfading());
}

TEST_CASE ("EqBand measured impulse response matches the design for every type", "[eqband]")
{
    struct Case { double sampleRate, centreHz; };
    const Case cases[] { { 44100.0, 100.0 }, { 44100.0, 1000.0 }, { 48000.0, 1000.0 },
                         { 96000.0, 1000.0 }, { 96000.0, 5000.0 }, { 192000.0, 20000.0 } };

    for (const auto& c : cases)
    {
        const auto numSamples = c.sampleRate > 100000.0 ? (1 << 18) : (1 << 17);

        std::vector<BandSettings> configurations;
        for (auto type : allTypes)
            if (! FilterTypes::usesSlope (type))
                configurations.push_back (make (type, c.centreHz));

        for (int slope = 0; slope < CutSlope::count; ++slope)
        {
            configurations.push_back (make (FilterType::lowCut, c.centreHz, 0.0, 0.71, slope));
            configurations.push_back (make (FilterType::highCut, c.centreHz, 0.0, 0.71, slope));
        }

        for (const auto& settings : configurations)
        {
            EqBand band;
            band.setTargets (settings);
            band.prepare (c.sampleRate, 2);

            juce::AudioBuffer<float> buffer (2, numSamples);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);
            buffer.setSample (1, 0, 1.0f);
            band.process (buffer);

            const auto design = BandDesign::design (settings, c.sampleRate);

            std::vector<double> points { c.centreHz / 4.0, c.centreHz / 2.0, c.centreHz, c.centreHz * 2.0, c.centreHz * 4.0 };

            for (auto f : points)
            {
                if (f >= c.sampleRate / 2.0)
                    continue;

                const auto expected = design.magnitudeDb (f, c.sampleRate);

                // Below -80 dB the float output buffer can no longer resolve the response.
                if (expected < -80.0)
                    continue;

                INFO ("type=" << FilterTypes::names[static_cast<int> (settings.type)] << " slope=" << settings.slopeIndex
                              << " fs=" << c.sampleRate << " f0=" << c.centreHz << " f=" << f);

                for (int ch = 0; ch < 2; ++ch)
                    CHECK_THAT (dftMagnitudeDb (buffer.getReadPointer (ch), numSamples, f, c.sampleRate),
                                WithinAbs (expected, toleranceDb));
            }
        }
    }
}

TEST_CASE ("EqBand survives a 20 Hz to 20 kHz sweep in one second for every type", "[eqband]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const auto totalSamples = static_cast<int> (sampleRate);

    std::vector<BandSettings> configurations;
    for (auto type : allTypes)
        configurations.push_back (make (type, 20.0, 12.0, 4.0, 3));
    configurations.push_back (make (FilterType::lowCut, 20.0, 0.0, 0.71, CutSlope::brickwallIndex));
    configurations.push_back (make (FilterType::highCut, 20.0, 0.0, 0.71, CutSlope::brickwallIndex));

    for (const auto& start : configurations)
    {
        EqBand band;
        band.setTargets (start);
        band.prepare (sampleRate, 2);

        const auto signal = run (band, sampleRate, totalSamples, [&] (int n)
        {
            auto s = start;
            s.frequencyHz = 20.0 * std::pow (1000.0, static_cast<double> (n) / totalSamples);
            band.setTargets (s);
        });

        INFO ("type=" << FilterTypes::names[static_cast<int> (start.type)] << " slope=" << start.slopeIndex << " fs=" << sampleRate);
        CHECK (signal.allFinite);
        CHECK (signal.largestStep < maxStep);
    }
}

TEST_CASE ("EqBand ramps a gain jump over the smoothing time", "[eqband]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const auto rampSamples = static_cast<int> (std::ceil (EqBand::rampSeconds * sampleRate));

    EqBand band;
    band.setTargets (make (FilterType::bell, 1000.0, 0.0, 1.0));
    band.prepare (sampleRate, 2);
    band.setTargets (make (FilterType::bell, 1000.0, 12.0, 1.0));

    juce::AudioBuffer<float> firstHalf (2, rampSamples / 2);
    fillSine (firstHalf, 1000.0, 0.25, sampleRate, 0);
    band.process (firstHalf);

    const auto midGain = band.getActiveCascade().magnitudeDb (1000.0, sampleRate);
    INFO ("fs=" << sampleRate << " mid-ramp gain=" << midGain);
    CHECK (midGain > 1.0);
    CHECK (midGain < 11.0);
    CHECK_FALSE (band.isCrossfading());   // continuous changes never crossfade

    juce::AudioBuffer<float> secondHalf (2, rampSamples - rampSamples / 2 + EqBand::subBlockSize);
    fillSine (secondHalf, 1000.0, 0.25, sampleRate, rampSamples / 2);
    band.process (secondHalf);

    CHECK (sameCascade (band.getActiveCascade(), BandDesign::design (make (FilterType::bell, 1000.0, 12.0, 1.0), sampleRate)));

    Signal signal;
    signal.add (firstHalf);
    signal.add (secondHalf);
    CHECK (signal.largestStep < maxStep);
}

TEST_CASE ("EqBand crossfades a type change", "[eqband]")
{
    const auto sampleRate = GENERATE (44100.0, 96000.0);
    const auto fadeSamples = static_cast<int> (std::ceil (EqBand::crossfadeSeconds * sampleRate));

    const auto from = make (FilterType::bell, 1000.0, 12.0, 1.0);
    const auto to = make (FilterType::lowCut, 1000.0, 0.0, 0.71, 3);

    EqBand band;
    band.setTargets (from);
    band.prepare (sampleRate, 2);

    bool sawFade = false;
    const auto signal = run (band, sampleRate, static_cast<int> (0.2 * sampleRate), [&] (int n)
    {
        if (n == 4096)
            band.setTargets (to);

        sawFade = sawFade || band.isCrossfading();
    });

    INFO ("fs=" << sampleRate);
    CHECK (sawFade);
    CHECK_FALSE (band.isCrossfading());
    CHECK (signal.allFinite);
    CHECK (signal.largestStep < maxStep);
    CHECK (sameCascade (band.getActiveCascade(), BandDesign::design (to, sampleRate)));

    // The fade takes crossfadeSeconds (plus at most one sub-block and one host block).
    EqBand timed;
    timed.setTargets (from);
    timed.prepare (sampleRate, 2);
    timed.setTargets (to);

    int processed = 0;
    juce::AudioBuffer<float> block (2, 16);
    do
    {
        fillSine (block, 1000.0, 0.25, sampleRate, processed);
        timed.process (block);
        processed += 16;
    }
    while (timed.isCrossfading() && processed < 10 * fadeSamples);

    CHECK (processed >= fadeSamples);
    CHECK (processed <= fadeSamples + EqBand::subBlockSize + 16);
}

TEST_CASE ("EqBand crossfades a slope change", "[eqband]")
{
    EqBand band;
    band.setTargets (make (FilterType::highCut, 2000.0, 0.0, 0.71, 1));
    band.prepare (48000.0, 2);

    const auto to = make (FilterType::highCut, 2000.0, 0.0, 0.71, CutSlope::brickwallIndex);
    const auto signal = run (band, 48000.0, 9600, [&] (int n) { if (n == 2048) band.setTargets (to); });

    CHECK (signal.allFinite);
    CHECK (signal.largestStep < maxStep);
    CHECK (sameCascade (band.getActiveCascade(), BandDesign::design (to, 48000.0)));
}

TEST_CASE ("EqBand disabled passes audio through bit-exactly", "[eqband]")
{
    SECTION ("disabled from the start")
    {
        EqBand band;
        band.setTargets (make (FilterType::bell, 1000.0, 12.0, 1.0, 3, false));
        band.prepare (48000.0, 2);

        juce::AudioBuffer<float> buffer (2, 512), original;
        fillSine (buffer, 1000.0, 0.25, 48000.0, 0);
        original.makeCopyOf (buffer);
        band.process (buffer);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                REQUIRE (juce::exactlyEqual (buffer.getSample (ch, i), original.getSample (ch, i)));
    }

    SECTION ("switched off while running")
    {
        EqBand band;
        band.setTargets (make (FilterType::bell, 1000.0, 12.0, 1.0));
        band.prepare (48000.0, 2);

        const auto signal = run (band, 48000.0, 4800, [&] (int n)
        {
            if (n == 1024)
                band.setTargets (make (FilterType::bell, 1000.0, 12.0, 1.0, 3, false));
        });

        CHECK (signal.largestStep < maxStep);
        CHECK_FALSE (band.isCrossfading());
        CHECK (band.getActiveCascade().numSections == 0);

        juce::AudioBuffer<float> buffer (2, 256), original;
        fillSine (buffer, 1000.0, 0.25, 48000.0, 4800);
        original.makeCopyOf (buffer);
        band.process (buffer);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                REQUIRE (juce::exactlyEqual (buffer.getSample (ch, i), original.getSample (ch, i)));
    }
}

TEST_CASE ("EqBand settles on the latest request after rapid discrete changes", "[eqband]")
{
    constexpr double sampleRate = 44100.0;
    const auto every5ms = static_cast<int> (0.005 * sampleRate);

    EqBand band;
    band.setTargets (make (FilterType::bell, 1000.0, 12.0, 4.0));
    band.prepare (sampleRate, 2);

    BandSettings last;
    int changes = 0;
    int nextChange = 0;

    const auto stopChanging = static_cast<int> (0.8 * sampleRate);
    bool finalSent = false;

    const auto signal = run (band, sampleRate, static_cast<int> (sampleRate), [&] (int n)
    {
        if (n >= nextChange && n < stopChanging)
        {
            last = make (allTypes[static_cast<size_t> (changes % static_cast<int> (allTypes.size()))], 1000.0, 12.0, 4.0,
                         changes % CutSlope::count, changes % 7 != 6);
            band.setTargets (last);
            ++changes;
            nextChange += every5ms;
        }
        else if (n >= stopChanging && ! finalSent)
        {
            // A final, non-trivial request (16 sections) that must be what the band ends on.
            last = make (FilterType::flatTilt, 1000.0, 12.0, 4.0);
            band.setTargets (last);
            finalSent = true;
        }
    });

    CHECK (signal.allFinite);
    CHECK (signal.largestStep < maxStep);
    CHECK_FALSE (band.isCrossfading());
    CHECK (sameCascade (band.getActiveCascade(), BandDesign::design (last, sampleRate)));
}

TEST_CASE ("EqBand reset clears the filter state", "[eqband]")
{
    EqBand band;
    band.setTargets (make (FilterType::bell, 200.0, 18.0, 4.0));
    band.prepare (48000.0, 2);

    juce::AudioBuffer<float> buffer (2, 512);
    fillSine (buffer, 200.0, 0.5, 48000.0, 0);
    band.process (buffer);

    band.reset();
    buffer.clear();
    band.process (buffer);

    CHECK (juce::exactlyEqual (buffer.getMagnitude (0, buffer.getNumSamples()), 0.0f));
}
