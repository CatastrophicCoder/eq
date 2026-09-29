#include "TestParameters.h"

#include "PluginEditor.h"
#include "dsp/BandDesign.h"
#include "dsp/CurveFitter.h"
#include "dsp/CutSlope.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>

using Catch::Matchers::WithinAbs;
using namespace TestParameters;

namespace
{
    constexpr double fs = 48000.0;

    BandSettings make (FilterType type, double f, double gain, double q, int slope = 3)
    {
        BandSettings s;
        s.type = type; s.frequencyHz = f; s.gainDb = gain; s.q = q; s.slopeIndex = slope;
        return s;
    }

    std::vector<double> grid (double lo = 20.0, double hi = 20000.0, int n = 256)
    {
        std::vector<double> f;
        for (int i = 0; i < n; ++i)
            f.push_back (lo * std::pow (hi / lo, i / (n - 1.0)));
        return f;
    }

    double sumDb (const std::vector<BandSettings>& bands, double f)
    {
        double sum = 0.0;
        for (const auto& b : bands)
            if (b.isActive())
                sum += BandDesign::design (b, fs).magnitudeDb (f, fs);
        return sum;
    }

    CurveFitter::Problem problemFrom (const std::vector<BandSettings>& truth, double lo = 20.0, double hi = 20000.0)
    {
        CurveFitter::Problem p;
        p.sampleRate = fs;
        p.rangeLowHz = lo;
        p.rangeHighHz = hi;
        for (auto f : grid())
        {
            const auto inside = f >= lo && f <= hi;
            p.frequenciesHz.push_back (f);
            p.targetDb.push_back (inside ? sumDb (truth, f) : 0.0);
            p.weights.push_back (inside ? 1.0 : CurveFitter::outsideWeight);
        }
        return p;
    }

    double worstError (const CurveFitter::Problem& p, const std::vector<BandSettings>& fitted, bool inside)
    {
        double worst = 0.0;
        for (size_t i = 0; i < p.frequenciesHz.size(); ++i)
        {
            const auto f = p.frequenciesHz[i];
            if ((f >= p.rangeLowHz && f <= p.rangeHighHz) != inside)
                continue;
            worst = std::max (worst, std::abs (sumDb (fitted, f) - p.targetDb[i]));
        }
        return worst;
    }

    bool hasType (const std::vector<BandSettings>& bands, FilterType t)
    {
        return std::any_of (bands.begin(), bands.end(), [t] (const BandSettings& b) { return b.type == t; });
    }

    /** Stated bounds (dB), see docs/PROGRESS.md. */
    constexpr double singleBandBoundDb = 0.1;
    constexpr double combinationBoundDb = 1.0;
    constexpr double outsideBoundDb = 1.0;
}

//==============================================================================
TEST_CASE ("The fitter uses exactly the slots it is given", "[fitter]")
{
    const auto p = problemFrom ({ make (FilterType::bell, 1000.0, 6.0, 2.0) });
    for (int slots : { 1, 3, 7 })
    {
        const auto fitted = CurveFitter::fit (p, slots);
        CHECK (static_cast<int> (fitted.size()) == slots);
        for (const auto& b : fitted)
        {
            CHECK (b.isActive());
            CHECK (b.channel == ChannelMode::stereo);
            CHECK_FALSE (b.dynamics.on);
        }
    }
}

TEST_CASE ("A single bell is fitted almost exactly", "[fitter]")
{
    for (auto truth : { make (FilterType::bell, 1000.0, 6.0, 2.0), make (FilterType::bell, 150.0, -9.0, 4.0),
                        make (FilterType::bell, 8000.0, 4.0, 0.7) })
    {
        const auto p = problemFrom ({ truth });
        const auto fitted = CurveFitter::fit (p, 1);
        const auto worst = worstError (p, fitted, true);
        INFO ("bell " << truth.frequencyHz << " Hz: worst " << worst << " dB");
        CHECK (worst <= singleBandBoundDb);
    }
}

TEST_CASE ("Combinations are fitted within the stated bound, with shelves and cuts at the ends", "[fitter]")
{
    struct Case { const char* name; std::vector<BandSettings> truth; int slots; FilterType endType; };
    const Case cases[] {
        { "low shelf + two bells", { make (FilterType::lowShelf, 120.0, 4.0, 0.71), make (FilterType::bell, 1000.0, -5.0, 1.5),
                                     make (FilterType::bell, 4000.0, 3.0, 3.0) }, 5, FilterType::lowShelf },
        { "bells + high cut 24 dB/oct", { make (FilterType::bell, 600.0, 4.0, 1.0), make (FilterType::bell, 2500.0, -3.0, 2.0),
                                          make (FilterType::highCut, 9000.0, 0.0, 0.71, 3) }, 6, FilterType::highCut },
        { "low cut 48 dB/oct + high shelf", { make (FilterType::lowCut, 60.0, 0.0, 0.71, 7), make (FilterType::highShelf, 6000.0, -4.0, 0.71),
                                              make (FilterType::bell, 1500.0, 2.0, 1.0) }, 6, FilterType::lowCut },
    };

    for (const auto& c : cases)
    {
        const auto p = problemFrom (c.truth);
        const auto fitted = CurveFitter::fit (p, c.slots);
        const auto worst = worstError (p, fitted, true);

        // Near a cut's deep stopband, compare only where the target is above -40 dB.
        double worstAbove = 0.0;
        for (size_t i = 0; i < p.frequenciesHz.size(); ++i)
            if (p.targetDb[i] > -40.0)
                worstAbove = std::max (worstAbove, std::abs (sumDb (fitted, p.frequenciesHz[i]) - p.targetDb[i]));

        INFO (c.name << ": worst " << worst << " dB (above -40 dB: " << worstAbove << " dB)");
        CHECK (worstAbove <= combinationBoundDb);
        CHECK (hasType (fitted, c.endType));
    }
}

TEST_CASE ("A partial range leaves the curve outside it nearly unchanged", "[fitter]")
{
    // Target only between 300 Hz and 3 kHz; outside, the fitted bands should add (almost) nothing.
    const auto p = problemFrom ({ make (FilterType::bell, 1000.0, 6.0, 1.5), make (FilterType::bell, 2000.0, -3.0, 2.0) }, 300.0, 3000.0);
    const auto fitted = CurveFitter::fit (p, 4);
    const auto inside = worstError (p, fitted, true);
    const auto outside = worstError (p, fitted, false);
    INFO ("inside " << inside << " dB, outside " << outside << " dB");
    CHECK (inside <= combinationBoundDb);
    CHECK (outside <= outsideBoundDb);
}

TEST_CASE ("A 16-band fit is fast enough to run on mouse release", "[fitter][cpu]")
{
    // A wavy target over the whole range.
    CurveFitter::Problem p;
    p.sampleRate = fs;
    p.rangeLowHz = 20.0;
    p.rangeHighHz = 20000.0;
    for (auto f : grid())
    {
        p.frequenciesHz.push_back (f);
        p.targetDb.push_back (6.0 * std::sin (3.0 * std::log2 (f / 20.0)));
        p.weights.push_back (1.0);
    }

    const auto start = juce::Time::getMillisecondCounterHiRes();
    const auto fitted = CurveFitter::fit (p, 16);
    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - start;
    WARN ("16-band fit in " << elapsed << " ms; worst " << worstError (p, fitted, true) << " dB");
    CHECK (fitted.size() == 16u);
    CHECK (elapsed < 1000.0);
}

//==============================================================================
namespace
{
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

    struct SketchFixture
    {
        juce::ScopedJuceInitialiser_GUI juce;
        ParametricEQAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> base { processor.createEditor() };
        ParametricEQAudioProcessorEditor& editor = *dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());
        ResponseDisplay& display = editor.getDisplay();
        const juce::ModifierKeys option { juce::ModifierKeys::altModifier };

        SketchFixture()
        {
            processor.setPlayConfigDetails (2, 2, fs, 512);
            processor.prepareToPlay (fs, 512);
            {
                UndoHistory::ScopedSuspend suspend (processor.getUndoHistory());
                setBand (processor, 2, FilterType::bell, 100.0f, 3.0f, 1.0f, 3, true);     // outside the sketch
                setBand (processor, 5, FilterType::bell, 1500.0f, -4.0f, 2.0f, 3, true);   // inside: replaced
            }
            processor.getUndoHistory().clear();
            editor.refreshControls();
        }

        juce::Point<float> at (double frequencyHz, double gainDb) const
        {
            const auto axis = display.getAxis();
            return { axis.xForFrequency (frequencyHz), axis.yForDb (gainDb) };
        }

        /** Option-drags a raised-cosine hump of the given height from lo to hi. */
        void sketch (double lo, double hi, double heightDb)
        {
            display.handlePress (at (lo, 0.0), option, 1);
            for (int i = 1; i <= 60; ++i)
            {
                const auto t = i / 60.0;
                const auto f = lo * std::pow (hi / lo, t);
                display.handleDrag (at (f, heightDb * 0.5 * (1.0 - std::cos (2.0 * juce::MathConstants<double>::pi * t))), option);
            }
            display.handleRelease();
            editor.refreshControls();
        }

        double curveDb (double f) const
        {
            double sum = 0.0;
            for (const auto& b : processor.getBandSettings())
                if (b.isActive())
                    sum += BandDesign::design (b, fs).magnitudeDb (f, fs);
            return sum;
        }
    };
}

TEST_CASE ("Option-drag sketches a curve: bands in its range are replaced, others kept, one undo step", "[sketch][editor][undo]")
{
    SketchFixture f;
    const auto before100 = f.curveDb (60.0);
    GestureCounter gestures (f.processor);

    f.sketch (400.0, 4000.0, 6.0);

    // Band 2 (100 Hz) untouched; band 5 (1.5 kHz, inside) replaced; the curve follows the sketch.
    CHECK_THAT (value (f.processor, "band2_freq"), WithinAbs (100.0f, 0.01f));
    CHECK_THAT (value (f.processor, "band2_gain"), WithinAbs (3.0f, 1e-4f));
    int inUse = 0;
    for (int b = 1; b <= 16; ++b)
        inUse += f.processor.isBandInUse (b) ? 1 : 0;
    CHECK (inUse == 16);   // all available slots: 14 free + 1 replaced, plus the kept band

    const auto middle = 400.0 * std::sqrt (10.0);   // the hump's top
    CHECK_THAT (f.curveDb (middle), WithinAbs (6.0, combinationBoundDb));
    CHECK_THAT (f.curveDb (60.0), WithinAbs (before100, outsideBoundDb));   // outside the sketch
    CHECK (gestures.allMatched());

    REQUIRE (f.processor.getUndoHistory().getNumSteps() == 1);
    f.processor.getUndoHistory().undo();
    CHECK (f.processor.isBandInUse (5));
    CHECK_THAT (value (f.processor, "band5_freq"), WithinAbs (1500.0f, 0.1f));
    CHECK_FALSE (f.processor.isBandInUse (1));
}

TEST_CASE ("A plain drag still selects; a very narrow sketch is ignored", "[sketch][editor]")
{
    SketchFixture f;

    f.display.handlePress (f.at (50.0, 10.0), {}, 1);
    f.display.handleDrag (f.at (3000.0, -10.0), {});
    CHECK (f.display.isSelectingArea());
    CHECK_FALSE (f.display.isSketching());
    f.display.handleRelease();

    f.display.handlePress (f.at (1000.0, 0.0), f.option, 1);
    CHECK (f.display.isSketching());
    f.display.handleDrag (f.at (1100.0, 5.0), f.option);   // about 1/7 octave
    f.display.handleRelease();
    CHECK_FALSE (f.display.isSketching());
    CHECK (f.processor.getUndoHistory().getNumSteps() == 0);
    CHECK (f.processor.isBandInUse (5));
}
