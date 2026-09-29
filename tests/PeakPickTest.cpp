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

        void feed()
        {
            BandSettings resonator;
            resonator.type = FilterType::bell;
            resonator.frequencyHz = 1000.0;
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
        CHECK (peaks[0].prominenceDb > 20.0);
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

TEST_CASE ("The marker sits on the spectrum's peak near the pointer, over empty space only", "[peakpick][editor]")
{
    Fixture f;

    f.display.handleHover (f.at (1100.0, 0.0));
    const auto marker = f.display.getPeakMarker();
    REQUIRE (marker.has_value());
    CHECK (std::abs (std::log2 (marker->frequencyHz / 1000.0)) < 1.0 / 12.0);

    // Pre+Post (default): the input spectrum.
    CHECK_THAT (marker->levelDb, WithinAbs (f.display.getPreAnalyzer().levelDb (marker->point), 1e-9));

    // Over a node: no marker.
    f.editor.refreshControls();
    f.display.handleHover (f.display.getNodes().front().position);
    CHECK_FALSE (f.display.getPeakMarker().has_value());

    // Post only: the output spectrum.
    auto settings = f.processor.getAnalyzerSettings();
    settings.mode = static_cast<int> (AnalyzerSettings::Mode::post);
    f.processor.setAnalyzerSettings (settings);
    f.display.handleHover (f.at (1100.0, 0.0));
    REQUIRE (f.display.getPeakMarker().has_value());
    CHECK_THAT (f.display.getPeakMarker()->levelDb, WithinAbs (f.display.getPostAnalyzer().levelDb (f.display.getPeakMarker()->point), 1e-9));

    // Analyzer off: none.
    settings.mode = static_cast<int> (AnalyzerSettings::Mode::off);
    f.processor.setAnalyzerSettings (settings);
    f.display.handleHover (f.at (1100.0, 0.0));
    CHECK_FALSE (f.display.getPeakMarker().has_value());
}

TEST_CASE ("Dragging the marker creates a bell at the peak, gain from the drag, as one undo step", "[peakpick][editor][undo]")
{
    Fixture f;
    f.display.handleHover (f.at (1100.0, 0.0));
    REQUIRE (f.display.getPeakMarker().has_value());
    const auto peak = *f.display.getPeakMarker();
    const auto start = f.display.getPeakMarkerPosition();

    GestureCounter gestures (f.processor);
    f.display.handlePress (start, {}, 1);
    for (int i = 1; i <= 8; ++i)
        f.display.handleDrag (start + juce::Point<float> (5.0f * static_cast<float> (i), -6.0f * static_cast<float> (i)), {});   // up (and a little right)
    f.display.handleRelease();

    REQUIRE (f.processor.isBandInUse (1));
    const auto band = f.processor.getBandSettings()[0];
    CHECK (band.type == FilterType::bell);
    CHECK_THAT (band.frequencyHz, WithinRel (peak.frequencyHz, 1e-3));   // frequency stays on the peak
    CHECK_THAT (band.q, WithinRel (std::clamp (peak.q, 0.5, 18.0), 1e-3));
    CHECK (band.q > 1.0);
    CHECK (band.gainDb > 1.0);   // dragged up: boost
    CHECK (band.isActive());
    CHECK (gestures.allMatched());
    CHECK (f.display.getSelection().getPrimary() == 1);

    REQUIRE (f.processor.getUndoHistory().getNumSteps() == 1);
    f.processor.getUndoHistory().undo();
    CHECK_FALSE (f.processor.isBandInUse (1));
}

TEST_CASE ("With all bands in use the marker shows the message instead of adding", "[peakpick][editor]")
{
    Fixture f;
    for (int b = 1; b <= 15; ++b)
        setBand (f.processor, b, FilterType::bell, 20.0f + static_cast<float> (b), 0.0f, 1.0f, 3, true);   // nodes far from 1 kHz
    f.editor.refreshControls();

    f.display.handleHover (f.at (1100.0, 0.0));
    CHECK_FALSE (f.display.getPeakMarker().has_value());   // nothing to add: no marker

    // Pressing where the marker would be still behaves as empty space.
    f.display.handlePress (f.at (1000.0, 0.0), {}, 1);
    f.display.handleRelease();
    CHECK (f.display.getSelection().getSelected().empty());
}
