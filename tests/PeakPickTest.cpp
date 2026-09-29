#include "TestParameters.h"

#include "PluginEditor.h"
#include "dsp/BandDesign.h"
#include "dsp/CascadeProcessor.h"
#include "ui/PeakFinder.h"
#include "ui/SpectrumAnalyzer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace TestParameters;

namespace
{
    std::vector<double> grid()
    {
        SpectrumAnalyzer a;
        std::vector<double> f;
        for (int k = 0; k < SpectrumAnalyzer::numPoints; ++k)
            f.push_back (a.frequency (k));
        return f;
    }

    /** A resonance on a flat floor: -40 dB + |analog band pass (f0, Q)| in dB, floor at -70 dB. */
    std::vector<double> resonance (const std::vector<double>& f, double f0, double q, double peakDb = -40.0)
    {
        std::vector<double> levels;
        for (auto x : f)
        {
            const auto r = x / f0 - f0 / x;
            levels.push_back (std::max (-70.0, peakDb - 10.0 * std::log10 (1.0 + q * q * r * r)));
        }
        return levels;
    }

    struct GestureCounter final : juce::AudioProcessorParameter::Listener
    {
        explicit GestureCounter (juce::AudioProcessor& p)
        {
            for (auto* param : p.getParameters()) { param->addListener (this); params.push_back (param); }
        }
        ~GestureCounter() override { for (auto* param : params) param->removeListener (this); }
        void parameterValueChanged (int, float) override {}
        void parameterGestureChanged (int index, bool starting) override { (starting ? begins : ends)[index]++; }
        bool allMatched() const
        {
            for (auto& [i, c] : begins) if (ends.count (i) == 0 || ends.at (i) != c) return false;
            return begins.size() == ends.size();
        }
        std::vector<juce::AudioProcessorParameter*> params;
        std::map<int, int> begins, ends;
    };

    /** Editor with noise through a strong 1 kHz resonance fed to the processor, so the analyzers show a peak. */
    struct Fixture
    {
        juce::ScopedJuceInitialiser_GUI juce;
        ParametricEQAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> base { processor.createEditor() };
        ParametricEQAudioProcessorEditor& editor = *dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());
        ResponseDisplay& display = editor.getDisplay();

        Fixture()
        {
            processor.setPlayConfigDetails (2, 2, 48000.0, 512);
            processor.prepareToPlay (48000.0, 512);
            setBand (processor, 16, FilterType::highShelf, 15000.0f, 0.0f, 0.71f, 3, true);   // a node far from 1 kHz
            processor.getUndoHistory().clear();
            feed();
        }

        void feed (double resonanceHz = 1000.0)
        {
            BandSettings resonator;
            resonator.type = FilterType::bell;
            resonator.frequencyHz = resonanceHz;
            resonator.gainDb = 24.0;
            resonator.q = 8.0;
            CascadeProcessor filter;
            filter.setCoefficients (BandDesign::design (resonator, 48000.0));

            juce::Random random (5);
            juce::MidiBuffer midi;
            for (int block = 0; block < 40; ++block)
            {
                juce::AudioBuffer<float> b (2, 512);
                for (int i = 0; i < 512; ++i)
                {
                    const auto x = static_cast<float> (0.02 * filter.processSample (0, random.nextFloat() - 0.5f));
                    b.setSample (0, i, x);
                    b.setSample (1, i, x);
                }
                processor.processBlock (b, midi);
                editor.refreshControls();
            }
        }

        juce::Point<float> at (double frequencyHz, double gainDb) const
        {
            const auto axis = display.getAxis();
            return { axis.xForFrequency (frequencyHz), axis.yForDb (gainDb) };
        }
    };
}

//==============================================================================
TEST_CASE ("The peak finder finds a resonance's frequency and Q", "[peakpick]")
{
    const auto f = grid();
    for (double q : { 1.0, 4.0, 12.0 })
    {
        const auto peaks = PeakFinder::find (f, resonance (f, 1000.0, q));
        INFO ("Q " << q);
        REQUIRE (peaks.size() == 1u);
        CHECK_THAT (peaks[0].frequencyHz, WithinRel (1000.0, 0.02));
        CHECK_THAT (peaks[0].q, WithinRel (q, 0.1));
        CHECK_THAT (peaks[0].levelDb, WithinAbs (-40.0, 0.1));
        // Prominence is measured within one octave: 10 log10 (1 + Q^2 (2 - 1/2)^2), at most the 30 dB to the floor.
        CHECK_THAT (peaks[0].prominenceDb, WithinAbs (std::min (30.0, 10.0 * std::log10 (1.0 + q * q * 2.25)), 0.5));
    }
}

TEST_CASE ("Bumps under 3 dB and flat spectra have no peaks", "[peakpick]")
{
    const auto f = grid();
    std::vector<double> flat (f.size(), -50.0);
    CHECK (PeakFinder::find (f, flat).empty());

    // A 2 dB bump: a Q 1 resonance compressed to 2 dB of height.
    auto bump = resonance (f, 500.0, 1.0);
    for (auto& v : bump)
        v = -50.0 + (v + 70.0) * 2.0 / 30.0;
    CHECK (PeakFinder::find (f, bump).empty());
}

TEST_CASE ("Near the pointer the most prominent peak wins", "[peakpick]")
{
    const auto f = grid();
    auto a = resonance (f, 150.0, 12.0, -60.0);   // 10 dB high
    const auto b = resonance (f, 260.0, 12.0, -50.0);  // 20 dB high
    for (size_t i = 0; i < a.size(); ++i)
        a[i] = std::max (a[i], b[i]);

    const auto peaks = PeakFinder::find (f, a);
    REQUIRE (peaks.size() == 2u);

    const auto near = PeakFinder::nearest (peaks, 200.0);   // both within half an octave
    REQUIRE (near.has_value());
    CHECK_THAT (near->frequencyHz, WithinRel (260.0, 0.02));

    CHECK_FALSE (PeakFinder::nearest (peaks, 2000.0).has_value());   // more than half an octave away
}

/** The ring on the 1 kHz resonance, if shown. */
static std::optional<ResponseDisplay::PeakRing> ringNear (const ResponseDisplay& display, double frequencyHz)
{
    for (const auto& r : display.getPeakRings())
        if (std::abs (std::log2 (r.peak.frequencyHz / frequencyHz)) < 1.0 / 12.0)
            return r;
    return std::nullopt;
}

TEST_CASE ("While hovering, rings mark the five most prominent peaks of the shown spectrum", "[peakpick][editor]")
{
    // Decision 2026-09-29 (second round): top peaks while hovering; nodes do not hide them.
    Fixture f;

    f.display.handleHover (f.at (300.0, 0.0));
    const auto& rings = f.display.getPeakRings();
    REQUIRE_FALSE (rings.empty());
    CHECK (rings.size() <= static_cast<size_t> (ResponseDisplay::maxPeakRings));
    const auto resonance = ringNear (f.display, 1000.0);
    REQUIRE (resonance.has_value());

    // The most prominent peaks: none shown is more prominent than one left out.
    double weakestShown = 1.0e9;
    for (const auto& r : rings)
        weakestShown = std::min (weakestShown, r.peak.prominenceDb);
    CHECK (resonance->peak.prominenceDb >= weakestShown);

    // Pre+Post (default): the input spectrum.
    CHECK_THAT (resonance->peak.levelDb, WithinAbs (f.display.getPreAnalyzer().levelDb (resonance->peak.point), 1e-9));

    // Over a node the rings stay.
    f.editor.refreshControls();
    f.display.handleHover (f.display.getNodes().front().position);
    CHECK_FALSE (f.display.getPeakRings().empty());

    // Post only: the output spectrum.
    auto settings = f.processor.getAnalyzerSettings();
    settings.mode = static_cast<int> (AnalyzerSettings::Mode::post);
    f.processor.setAnalyzerSettings (settings);
    f.display.handleHover (f.at (5000.0, 0.0));
    const auto post = ringNear (f.display, 1000.0);
    REQUIRE (post.has_value());
    CHECK_THAT (post->peak.levelDb, WithinAbs (f.display.getPostAnalyzer().levelDb (post->peak.point), 1e-9));

    // Analyzer off, or the pointer outside the display: none.
    settings.mode = static_cast<int> (AnalyzerSettings::Mode::off);
    f.processor.setAnalyzerSettings (settings);
    f.display.handleHover (f.at (1100.0, 0.0));
    CHECK (f.display.getPeakRings().empty());
    settings.mode = static_cast<int> (AnalyzerSettings::Mode::prePost);
    f.processor.setAnalyzerSettings (settings);
    f.display.handleHover (f.at (1100.0, 0.0));
    CHECK_FALSE (f.display.getPeakRings().empty());
    f.display.handleHover ({ -50.0f, -50.0f });
    CHECK (f.display.getPeakRings().empty());
}

TEST_CASE ("Dragging a ring creates a bell at the peak, gain from the drag, as one undo step", "[peakpick][editor][undo]")
{
    Fixture f;
    f.display.handleHover (f.at (1100.0, 0.0));
    const auto ring = ringNear (f.display, 1000.0);
    REQUIRE (ring.has_value());
    const auto peak = ring->peak;
    const auto start = ring->position;

    GestureCounter gestures (f.processor);
    f.display.handlePress (start, {}, 1);
    for (int i = 1; i <= 8; ++i)
        f.display.handleDrag (start + juce::Point<float> (5.0f * static_cast<float> (i), -6.0f * static_cast<float> (i)), {});
    f.display.handleRelease();

    REQUIRE (f.processor.isBandInUse (1));
    const auto band = f.processor.getBandSettings()[0];
    CHECK (band.type == FilterType::bell);
    CHECK_THAT (band.frequencyHz, WithinRel (peak.frequencyHz, 1e-3));   // frequency stays on the peak
    CHECK_THAT (band.q, WithinRel (std::clamp (peak.q, 0.5, 18.0), 1e-3));
    CHECK (band.q > 1.0);
    CHECK (band.gainDb > 1.0);
    CHECK (gestures.allMatched());
    CHECK (f.display.getSelection().getPrimary() == 1);

    REQUIRE (f.processor.getUndoHistory().getNumSteps() == 1);
    f.processor.getUndoHistory().undo();
    CHECK_FALSE (f.processor.isBandInUse (1));
}

TEST_CASE ("With all bands in use the rings stay; pressing one explains instead of adding", "[peakpick][editor]")
{
    Fixture f;
    for (int b = 1; b <= 15; ++b)
        setBand (f.processor, b, FilterType::bell, 20.0f + static_cast<float> (b), 0.0f, 1.0f, 3, true);   // nodes far from 1 kHz
    f.processor.getUndoHistory().clear();   // the setup marked bands in use, which counts as edits
    f.editor.refreshControls();

    f.display.handleHover (f.at (1100.0, 0.0));
    const auto ring = ringNear (f.display, 1000.0);
    REQUIRE (ring.has_value());

    f.display.handlePress (ring->position, {}, 1);
    f.display.handleRelease();
    CHECK (f.display.getMessage().contains ("All 16 bands in use"));
    CHECK (f.processor.getBandSettings()[0].frequencyHz < 30.0);   // band 1 untouched
    CHECK (f.processor.getUndoHistory().getNumSteps() == 0);
}

TEST_CASE ("A ring holds still while the pointer is near it, even as the spectrum moves", "[peakpick][editor]")
{
    // Owner feedback 2026-09-29: rings jumped with the live peaks and were hard to catch.
    Fixture f;
    f.display.handleHover (f.at (1100.0, 0.0));
    const auto held = ringNear (f.display, 1000.0);
    REQUIRE (held.has_value());

    // The pointer approaches from further away while the resonance moves: the ring stays.
    const auto start = f.at (1350.0, 6.0);
    for (int step = 1; step <= 10; ++step)
    {
        f.feed (step % 2 == 0 ? 1000.0 : 1180.0);
        const auto t = static_cast<float> (step) / 10.0f;
        f.display.handleHover (start + (held->position - start) * t);
        const auto now = ringNear (f.display, held->peak.frequencyHz);
        REQUIRE (now.has_value());
        CHECK (now->peak.point == held->peak.point);
        CHECK (now->position == held->position);
    }

    // A press within the hold radius picks the held peak.
    f.display.handlePress (held->position + juce::Point<float> (-20.0f, 15.0f), {}, 1);
    f.display.handleRelease();
    REQUIRE (f.processor.isBandInUse (1));
    CHECK_THAT (f.processor.getBandSettings()[0].frequencyHz, WithinRel (held->peak.frequencyHz, 1e-3));
}

TEST_CASE ("Rings away from the pointer follow the spectrum", "[peakpick][editor]")
{
    Fixture f;
    f.display.handleHover (f.at (100.0, 0.0));   // far from 1 kHz: nothing there is held
    REQUIRE (ringNear (f.display, 1000.0).has_value());

    f.feed (1300.0);
    f.display.handleHover (f.at (110.0, 0.0));
    CHECK (ringNear (f.display, 1300.0).has_value());
}
