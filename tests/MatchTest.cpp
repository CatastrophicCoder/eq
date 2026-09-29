#include "TestParameters.h"

#include "PluginEditor.h"
#include "dsp/BandDesign.h"
#include "dsp/CascadeProcessor.h"
#include "dsp/MatchCurve.h"
#include "dsp/SpectrumAverager.h"
#include "ui/MatchSession.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using Catch::Matchers::WithinAbs;
using namespace TestParameters;

namespace
{
    constexpr double fs = 48000.0;

    std::vector<double> grid (double lo = 20.0, double hi = 20000.0, int n = 256)
    {
        std::vector<double> f;
        for (int i = 0; i < n; ++i)
            f.push_back (lo * std::pow (hi / lo, i / (n - 1.0)));
        return f;
    }

    BandSettings make (FilterType type, double f, double gain, double q)
    {
        BandSettings s;
        s.type = type; s.frequencyHz = f; s.gainDb = gain; s.q = q;
        return s;
    }

    /** Pink noise (Kellet's refined filter on white noise), roughly unit level. */
    struct PinkNoise
    {
        juce::Random random { 11 };
        double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        float next()
        {
            const auto white = random.nextFloat() * 2.0 - 1.0;
            b0 = 0.99886 * b0 + white * 0.0555179; b1 = 0.99332 * b1 + white * 0.0750759;
            b2 = 0.96900 * b2 + white * 0.1538520; b3 = 0.86650 * b3 + white * 0.3104856;
            b4 = 0.55000 * b4 + white * 0.5329522; b5 = -0.7616 * b5 - white * 0.0168980;
            const auto pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362;
            b6 = white * 0.115926;
            return static_cast<float> (0.1 * pink);
        }
    };

    /** The known "reference EQ": what the match should find. */
    const std::vector<BandSettings> knownEq { make (FilterType::bell, 1000.0, 6.0, 1.0), make (FilterType::highShelf, 5000.0, -4.0, 0.71) };

    double knownEqDb (double f)
    {
        double sum = 0.0;
        for (const auto& b : knownEq)
            sum += BandDesign::design (b, fs).magnitudeDb (f, fs);
        return sum;
    }

    /** n seconds of pink noise, and the same noise through the known EQ. */
    struct Signals
    {
        std::vector<float> current, reference;
        explicit Signals (double seconds)
        {
            PinkNoise pink;
            std::vector<CascadeProcessor> filters (knownEq.size());
            for (size_t i = 0; i < knownEq.size(); ++i)
                filters[i].setCoefficients (BandDesign::design (knownEq[i], fs));

            const auto n = static_cast<int> (seconds * fs);
            for (int i = 0; i < n; ++i)
            {
                const auto x = pink.next();
                double y = x;
                for (auto& f : filters)
                    y = f.processSample (0, y);
                current.push_back (x);
                reference.push_back (static_cast<float> (y));
            }
        }
    };

    /** Stated bound (dB) between the match (curve or applied bands) and the known EQ's shape, 100 Hz - 10 kHz,
        for 6-8 s of pink noise at 1/6-octave smoothing; see docs/PROGRESS.md. Worst measured 0.057 dB. */
    constexpr double matchBoundDb = 0.25;
}

//==============================================================================
TEST_CASE ("The averager reads white noise flat, pink noise at -3 dB per octave and a sine at its level", "[match][averager]")
{
    SECTION ("white and pink")
    {
        SpectrumAverager white, pinkAverager;
        white.prepare (fs);
        pinkAverager.prepare (fs);
        juce::Random random (3);
        PinkNoise pink;
        std::vector<float> w, p;
        for (int i = 0; i < static_cast<int> (8 * fs); ++i)
        {
            w.push_back (random.nextFloat() - 0.5f);
            p.push_back (pink.next());
        }
        white.addSamples (w.data(), static_cast<int> (w.size()));
        pinkAverager.addSamples (p.data(), static_cast<int> (p.size()));
        CHECK_THAT (white.getSeconds(), WithinAbs (8.0, 0.2));

        const auto f = grid (100.0, 10000.0, 21);
        const auto wl = white.levelsDb (f);
        const auto pl = pinkAverager.levelsDb (f);

        // White: every point within 1 dB of the mean. Pink: the regression slope, and every point
        // within 1 dB of the regression line.
        double whiteMean = 0.0, sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
        const auto n = static_cast<double> (f.size());
        for (size_t i = 0; i < f.size(); ++i)
        {
            const auto x = std::log2 (f[i] / 100.0);
            whiteMean += wl[i] / n;
            sx += x; sy += pl[i]; sxx += x * x; sxy += x * pl[i];
        }
        const auto slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
        const auto intercept = (sy - slope * sx) / n;
        CHECK_THAT (slope, WithinAbs (-3.0103, 0.3));

        for (size_t i = 0; i < f.size(); ++i)
        {
            INFO (f[i] << " Hz: white " << wl[i] << " (mean " << whiteMean << "), pink " << pl[i]);
            CHECK_THAT (wl[i], WithinAbs (whiteMean, 1.0));
            CHECK_THAT (pl[i], WithinAbs (intercept + slope * std::log2 (f[i] / 100.0), 1.0));
        }
    }

    SECTION ("sine")
    {
        SpectrumAverager a;
        a.prepare (fs);
        std::vector<float> x;
        for (int i = 0; i < static_cast<int> (2 * fs); ++i)
            x.push_back (static_cast<float> (0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / fs)));
        a.addSamples (x.data(), static_cast<int> (x.size()));

        // Levels are band-averaged power (+-1/24 octave), so a tone reads below its peak level;
        // it must stand out clearly at its own frequency.
        const auto levels = a.levelsDb ({ 500.0, 1000.0, 2000.0 });
        INFO ("500 Hz " << levels[0] << ", 1 kHz " << levels[1] << ", 2 kHz " << levels[2]);
        CHECK (levels[1] > -20.0);
        CHECK (levels[1] > levels[0] + 40.0);
        CHECK (levels[1] > levels[2] + 40.0);
    }
}

TEST_CASE ("Fractional-octave smoothing widens a spike and leaves flat spectra flat", "[match]")
{
    const auto f = grid();
    std::vector<double> flat (f.size(), -30.0), spike (f.size(), -60.0);
    spike[128] = 0.0;

    for (double w : { 1.0 / 12.0, 1.0 / 3.0, 1.0 })
    {
        const auto sf = MatchCurve::smooth (f, flat, w);
        for (auto v : sf)
            CHECK_THAT (v, WithinAbs (-30.0, 1e-9));

        const auto ss = MatchCurve::smooth (f, spike, w);
        int above = 0;
        for (auto v : ss)
            above += v > -59.0 ? 1 : 0;
        const auto pointsPerOctave = (f.size() - 1) / std::log2 (1000.0);
        INFO ("width " << w << " octave: " << above << " points raised");
        CHECK (above >= static_cast<int> (w * pointsPerOctave) - 1);
        CHECK (above <= static_cast<int> (w * pointsPerOctave) + 3);
    }
}

TEST_CASE ("The match curve recovers a known EQ; amount scales it; level differences are ignored", "[match]")
{
    const Signals s (8.0);
    SpectrumAverager cur, ref, quieter;
    for (auto* a : { &cur, &ref, &quieter })
        a->prepare (fs);
    cur.addSamples (s.current.data(), static_cast<int> (s.current.size()));
    ref.addSamples (s.reference.data(), static_cast<int> (s.reference.size()));
    std::vector<float> scaled (s.reference);
    for (auto& v : scaled)
        v *= 0.5f;   // -6 dB overall
    quieter.addSamples (scaled.data(), static_cast<int> (scaled.size()));

    const auto f = grid();
    const auto full = MatchCurve::compute (f, ref.levelsDb (f), cur.levelsDb (f), 1.0, 1.0 / 6.0);
    const auto half = MatchCurve::compute (f, ref.levelsDb (f), cur.levelsDb (f), 0.5, 1.0 / 6.0);
    const auto level = MatchCurve::compute (f, quieter.levelsDb (f), cur.levelsDb (f), 1.0, 1.0 / 6.0);

    // Compare shapes (the curve's overall level is removed, so compare after removing the known EQ's too).
    auto meanOver = [&] (const std::function<double (size_t)>& v)
    {
        double sum = 0.0; int n = 0;
        for (size_t i = 0; i < f.size(); ++i)
            if (f[i] >= MatchCurve::levelLowHz && f[i] <= MatchCurve::levelHighHz) { sum += v (i); ++n; }
        return sum / n;
    };
    const auto knownMean = meanOver ([&] (size_t i) { return knownEqDb (f[i]); });

    double worst = 0.0;
    for (size_t i = 0; i < f.size(); ++i)
    {
        if (f[i] < 100.0 || f[i] > 10000.0)
            continue;
        worst = std::max (worst, std::abs (full[i] - (knownEqDb (f[i]) - knownMean)));
        CHECK_THAT (half[i], WithinAbs (0.5 * full[i], 1e-9));
        CHECK_THAT (level[i], WithinAbs (full[i], 1e-6));
    }
    WARN ("match curve vs known EQ (shape), 100 Hz - 10 kHz: worst " << worst << " dB");
    CHECK (worst <= matchBoundDb);
}

TEST_CASE ("The side-chain tap feeds the match only while learning with a side-chain connected", "[match][sidechain]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    auto layout = p.getBusesLayout();
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    REQUIRE (p.setBusesLayout (layout));
    p.prepareToPlay (fs, 512);
    CHECK (p.isSidechainConnected());

    juce::AudioBuffer<float> buffer (4, 512);
    buffer.clear();
    buffer.setSample (2, 0, 0.5f);
    juce::MidiBuffer midi;

    p.processBlock (buffer, midi);
    CHECK (p.getSidechainFifo().getNumReady() == 0);   // not learning

    p.setSidechainTapActive (true);
    p.processBlock (buffer, midi);
    CHECK (p.getSidechainFifo().getNumReady() == 512);

    ParametricEQAudioProcessor unconnected;
    unconnected.setPlayConfigDetails (2, 2, fs, 512);
    unconnected.prepareToPlay (fs, 512);
    CHECK_FALSE (unconnected.isSidechainConnected());
}

//==============================================================================
namespace
{
    struct MatchFixture
    {
        juce::ScopedJuceInitialiser_GUI juce;
        ParametricEQAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> base;
        ParametricEQAudioProcessorEditor* editor = nullptr;

        explicit MatchFixture (bool sidechain = false)
        {
            if (sidechain)
            {
                auto layout = processor.getBusesLayout();
                layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
                REQUIRE (processor.setBusesLayout (layout));
            }
            else
            {
                processor.setPlayConfigDetails (2, 2, fs, 512);
            }
            processor.prepareToPlay (fs, 512);
            openEditor();
        }

        void openEditor()
        {
            base.reset (processor.createEditor());
            editor = dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());
        }

        void closeEditor()
        {
            base.reset();
            editor = nullptr;
        }

        MatchSession& session() { return processor.getMatchSession(); }

        /** Plays main (and optionally side-chain) audio through the plugin, draining the taps as the editor would. */
        void play (const std::vector<float>& main, const std::vector<float>* side = nullptr)
        {
            juce::MidiBuffer midi;
            const auto channels = side != nullptr ? 4 : 2;
            for (size_t start = 0; start + 512 <= main.size(); start += 512)
            {
                juce::AudioBuffer<float> b (channels, 512);
                for (int i = 0; i < 512; ++i)
                {
                    b.setSample (0, i, main[start + static_cast<size_t> (i)]);
                    b.setSample (1, i, main[start + static_cast<size_t> (i)]);
                    if (side != nullptr)
                    {
                        b.setSample (2, i, (*side)[start + static_cast<size_t> (i)]);
                        b.setSample (3, i, (*side)[start + static_cast<size_t> (i)]);
                    }
                }
                processor.processBlock (b, midi);
                if ((start / 512) % 8 == 0)
                    editor->refreshControls();
            }
            editor->refreshControls();
        }

        double curveDb (double f) const
        {
            double sum = 0.0;
            for (const auto& b : processor.getBandSettings())
                if (b.isActive())
                    sum += BandDesign::design (b, fs).magnitudeDb (f, fs);
            return sum;
        }

        /** Worst deviation of the plugin's curve from the known EQ's shape (levels removed), 100 Hz - 10 kHz. */
        double worstShapeError() const
        {
            const auto f = grid (100.0, 10000.0, 60);
            double curveMean = 0.0, knownMean = 0.0;
            for (auto x : f) { curveMean += curveDb (x); knownMean += knownEqDb (x); }
            curveMean /= static_cast<double> (f.size());
            knownMean /= static_cast<double> (f.size());
            double worst = 0.0;
            for (auto x : f)
                worst = std::max (worst, std::abs ((curveDb (x) - curveMean) - (knownEqDb (x) - knownMean)));
            return worst;
        }
    };
}

TEST_CASE ("Capture passes, then Replace all: the plugin's curve matches the known EQ, as one undo step", "[match][editor][undo]")
{
    MatchFixture f;
    const Signals s (6.0);
    auto& session = f.session();
    CHECK_FALSE (session.usesSidechain());
    CHECK_FALSE (session.canApply());

    session.startLearning (MatchSession::Learning::reference);
    f.play (s.reference);
    session.stopLearning();
    CHECK_THAT (session.getReferenceSeconds(), WithinAbs (6.0, 0.3));

    session.startLearning (MatchSession::Learning::current);
    f.play (s.current);
    session.stopLearning();
    CHECK_THAT (session.getCurrentSeconds(), WithinAbs (6.0, 0.3));
    REQUIRE (session.canApply());

    session.setSmoothingOctaves (1.0 / 6.0);
    session.apply (MatchSession::ApplyMode::replaceAll);

    int inUse = 0;
    for (int b = 1; b <= 16; ++b)
        inUse += f.processor.isBandInUse (b) ? 1 : 0;
    CHECK (inUse == 16);
    const auto worst = f.worstShapeError();
    WARN ("applied match (capture, replace all) vs known EQ shape: worst " << worst << " dB");
    CHECK (worst <= matchBoundDb);

    REQUIRE (f.processor.getUndoHistory().getNumSteps() == 1);
    f.processor.getUndoHistory().undo();
    for (int b = 1; b <= 16; ++b)
        CHECK_FALSE (f.processor.isBandInUse (b));
}

TEST_CASE ("Keep existing: existing bands stay, the match uses the free slots and accounts for them", "[match][editor]")
{
    MatchFixture f;
    {
        UndoHistory::ScopedSuspend suspend (f.processor.getUndoHistory());
        setBand (f.processor, 3, FilterType::bell, 1000.0f, 3.0f, 1.0f, 3, true);   // half of the known bell already
    }
    const Signals s (6.0);
    auto& session = f.session();
    session.startLearning (MatchSession::Learning::reference);
    f.play (s.reference);
    session.startLearning (MatchSession::Learning::current);   // starting the next pass ends the previous one
    f.play (s.current);
    session.stopLearning();
    CHECK (session.freeSlots() == 15);

    session.setSmoothingOctaves (1.0 / 6.0);
    session.apply (MatchSession::ApplyMode::keepExisting);
    CHECK_THAT (value (f.processor, "band3_gain"), WithinAbs (3.0f, 1e-4f));
    CHECK_THAT (value (f.processor, "band3_freq"), WithinAbs (1000.0f, 0.01f));
    const auto worst = f.worstShapeError();
    WARN ("applied match (keep existing) vs known EQ shape: worst " << worst << " dB");
    CHECK (worst <= matchBoundDb);
}

TEST_CASE ("With a side-chain, reference and current are learned in one pass", "[match][editor][sidechain]")
{
    MatchFixture f (true);
    const Signals s (6.0);
    auto& session = f.session();
    REQUIRE (session.usesSidechain());

    session.startLearning (MatchSession::Learning::both);
    f.play (s.current, &s.reference);
    session.stopLearning();
    CHECK_THAT (session.getReferenceSeconds(), WithinAbs (6.0, 0.3));
    CHECK_THAT (session.getCurrentSeconds(), WithinAbs (6.0, 0.3));

    session.setSmoothingOctaves (1.0 / 6.0);
    session.apply (MatchSession::ApplyMode::replaceAll);
    const auto worst = f.worstShapeError();
    WARN ("applied match (side-chain) vs known EQ shape: worst " << worst << " dB");
    CHECK (worst <= matchBoundDb);
}

TEST_CASE ("The match window opens from the bottom bar and the display previews the match", "[match][editor]")
{
    MatchFixture f;
    auto& bar = f.editor->getBottomBar();
    REQUIRE (bar.getMatchButton().onClick != nullptr);
    CHECK_FALSE (f.editor->isMatchWindowOpen());
    bar.getMatchButton().onClick();
    CHECK (f.editor->isMatchWindowOpen());
    CHECK_FALSE (f.editor->getDisplay().hasMatchPreview());   // nothing learned yet

    const Signals s (3.0);
    auto& session = f.session();
    session.startLearning (MatchSession::Learning::reference);
    f.play (s.reference);
    session.startLearning (MatchSession::Learning::current);
    f.play (s.current);
    session.stopLearning();
    f.editor->refreshControls();
    CHECK (f.editor->getDisplay().hasMatchPreview());

    // The panel's Amount slider changes the preview.
    auto& panel = f.editor->getMatchPanel();
    const auto before = f.editor->getDisplay().getMatchPreviewDb (1000.0);
    panel.getAmountSlider().setValue (50.0, juce::sendNotificationSync);
    f.editor->refreshControls();
    CHECK_THAT (f.editor->getDisplay().getMatchPreviewDb (1000.0), WithinAbs (0.5 * before, 0.05));

    bar.getMatchButton().onClick();   // toggles closed
    CHECK_FALSE (f.editor->isMatchWindowOpen());
    f.editor->refreshControls();
    CHECK_FALSE (f.editor->getDisplay().hasMatchPreview());
}

//==============================================================================
namespace
{
    void learnBoth (MatchFixture& f, const Signals& s)
    {
        f.session().startLearning (MatchSession::Learning::reference);
        f.play (s.reference);
        f.session().startLearning (MatchSession::Learning::current);
        f.play (s.current);
        f.session().stopLearning();
    }
}

TEST_CASE ("Learned spectra live in the plugin and survive closing its window", "[match][state]")
{
    MatchFixture f;
    const Signals s (3.0);
    learnBoth (f, s);
    REQUIRE (f.session().canApply());
    const auto seconds = f.session().getReferenceSeconds();
    const auto curve = f.session().curveDb (grid());

    f.closeEditor();
    CHECK (f.session().canApply());
    f.openEditor();
    CHECK (f.editor->getMatchSession().canApply());
    CHECK_THAT (f.session().getReferenceSeconds(), WithinAbs (seconds, 1e-9));
    CHECK (f.session().curveDb (grid()) == curve);

    f.editor->setMatchWindowOpen (true);
    f.editor->refreshControls();
    CHECK (f.editor->getDisplay().hasMatchPreview());
    f.editor->setMatchWindowOpen (false);
}

TEST_CASE ("Closing the plugin window stops a running learn pass and keeps what it learned", "[match][state]")
{
    MatchFixture f;
    const Signals s (2.0);
    f.session().startLearning (MatchSession::Learning::reference);
    f.play (s.reference);
    f.closeEditor();
    CHECK (f.session().getLearning() == MatchSession::Learning::none);
    CHECK_THAT (f.session().getReferenceSeconds(), WithinAbs (2.0, 0.2));
}

TEST_CASE ("Learned spectra, Amount and Smoothing are saved with the project", "[match][state]")
{
    juce::MemoryBlock saved;
    std::vector<double> curve;
    double referenceSeconds = 0.0, currentSeconds = 0.0;
    {
        MatchFixture f;
        const Signals s (4.0);
        learnBoth (f, s);
        f.session().setAmount (0.7);
        f.session().setSmoothingOctaves (1.0 / 6.0);
        curve = f.session().curveDb (grid());
        referenceSeconds = f.session().getReferenceSeconds();
        currentSeconds = f.session().getCurrentSeconds();
        f.processor.getStateInformation (saved);
    }

    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor restored;
    restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    auto& session = restored.getMatchSession();
    REQUIRE (session.canApply());
    CHECK_THAT (session.getReferenceSeconds(), WithinAbs (referenceSeconds, 0.01));
    CHECK_THAT (session.getCurrentSeconds(), WithinAbs (currentSeconds, 0.01));
    CHECK_THAT (session.getAmount(), WithinAbs (0.7, 1e-9));
    CHECK_THAT (session.getSmoothingOctaves(), WithinAbs (1.0 / 6.0, 1e-9));

    const auto again = session.curveDb (grid());
    double worst = 0.0;
    for (size_t i = 0; i < curve.size(); ++i)
        worst = std::max (worst, std::abs (again[i] - curve[i]));
    INFO ("match curve after save and reload: worst difference " << worst << " dB");
    CHECK (worst <= 0.01);

    // State version 6; the EQ Match element stays small.
    const auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), static_cast<int> (saved.getSize()));
    REQUIRE (xml != nullptr);
    CHECK (xml->getIntAttribute ("stateVersion") == 6);
    const auto* match = xml->getChildByName (MatchSession::stateTag);
    REQUIRE (match != nullptr);
    CHECK (match->toString().length() < 16000);
}

TEST_CASE ("Older sessions open with nothing learned; A/B switches leave the learned spectra alone", "[match][state]")
{
    juce::MemoryBlock saved;
    {
        MatchFixture f;
        const Signals s (2.0);
        learnBoth (f, s);
        REQUIRE (f.session().canApply());

        f.processor.getAbComparison().switchTo (AbComparison::Slot::b);
        CHECK (f.session().canApply());
        f.processor.getStateInformation (saved);
    }

    auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), static_cast<int> (saved.getSize()));
    REQUIRE (xml != nullptr);
    xml->deleteAllChildElementsWithTagName (MatchSession::stateTag);
    xml->setAttribute ("stateVersion", 5);
    juce::MemoryBlock v5;
    juce::AudioProcessor::copyXmlToBinary (*xml, v5);

    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor restored;
    restored.setStateInformation (v5.getData(), static_cast<int> (v5.getSize()));
    CHECK_FALSE (restored.getMatchSession().canApply());
}
