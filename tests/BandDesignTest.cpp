#include "dsp/BandDesign.h"
#include "dsp/ButterworthCascade.h"
#include "dsp/CutSlope.h"
#include "dsp/FlatTiltDesign.h"
#include "dsp/MatchedAllpassDesign.h"
#include "dsp/MatchedBandpassDesign.h"
#include "dsp/MatchedNotchDesign.h"
#include "dsp/MatchedPeakingDesign.h"
#include "dsp/MatchedShelfDesign.h"
#include "dsp/TiltShelfDesign.h"

#include <juce_core/juce_core.h>

#include <catch2/catch_test_macros.hpp>

namespace
{
    bool equal (const BiquadCoefficients& a, const BiquadCoefficients& b)
    {
        // Exact on purpose: the dispatch must call the very same design.
        return juce::exactlyEqual (a.b0, b.b0) && juce::exactlyEqual (a.b1, b.b1) && juce::exactlyEqual (a.b2, b.b2)
            && juce::exactlyEqual (a.a1, b.a1) && juce::exactlyEqual (a.a2, b.a2);
    }

    bool equal (const SectionCascade& a, const SectionCascade& b)
    {
        if (a.numSections != b.numSections)
            return false;

        for (int i = 0; i < a.numSections; ++i)
            if (! equal (a.sections[static_cast<size_t> (i)], b.sections[static_cast<size_t> (i)]))
                return false;

        return true;
    }

    SectionCascade single (const BiquadCoefficients& c)
    {
        SectionCascade s;
        s.add (c);
        return s;
    }

    BandSettings settings (FilterType type, double f = 2500.0, double gain = 7.5, double q = 1.8, int slope = 3)
    {
        BandSettings s;
        s.type = type;
        s.frequencyHz = f;
        s.gainDb = gain;
        s.q = q;
        s.slopeIndex = slope;
        return s;
    }

    constexpr double fs = 48000.0;
}

TEST_CASE ("Filter type order and names are fixed", "[banddesign]")
{
    // The index is saved in sessions (band<n>_type): this order must never change.
    CHECK (static_cast<int> (FilterType::bell) == 0);
    CHECK (static_cast<int> (FilterType::lowShelf) == 1);
    CHECK (static_cast<int> (FilterType::highShelf) == 2);
    CHECK (static_cast<int> (FilterType::lowCut) == 3);
    CHECK (static_cast<int> (FilterType::highCut) == 4);
    CHECK (static_cast<int> (FilterType::notch) == 5);
    CHECK (static_cast<int> (FilterType::bandPass) == 6);
    CHECK (static_cast<int> (FilterType::tiltShelf) == 7);
    CHECK (static_cast<int> (FilterType::flatTilt) == 8);
    CHECK (static_cast<int> (FilterType::allPass) == 9);
    CHECK (FilterTypes::count == 10);
    CHECK (juce::String (FilterTypes::names[3]) == "Low Cut");
}

TEST_CASE ("Cut slope indices map to Butterworth orders", "[banddesign]")
{
    CHECK (CutSlope::count == 17);

    for (int i = 0; i < 16; ++i)
    {
        CHECK (CutSlope::order (i) == i + 1);
        CHECK (CutSlope::dbPerOctave (i) == 6 * (i + 1));
    }

    CHECK (CutSlope::order (CutSlope::brickwallIndex) == 32);
    CHECK (juce::String (CutSlope::labels[0]) == "6 dB/oct");
    CHECK (juce::String (CutSlope::labels[15]) == "96 dB/oct");
    CHECK (juce::String (CutSlope::labels[16]) == "Brickwall");
}

TEST_CASE ("BandDesign dispatches each type to its design", "[banddesign]")
{
    using Kind = ButterworthCascade::Kind;

    CHECK (equal (BandDesign::design (settings (FilterType::bell), fs),
                  single (MatchedPeakingDesign::design (2500.0, 7.5, 1.8, fs))));
    CHECK (equal (BandDesign::design (settings (FilterType::lowShelf), fs),
                  single (MatchedShelfDesign::designLow (2500.0, 7.5, fs))));
    CHECK (equal (BandDesign::design (settings (FilterType::highShelf), fs),
                  single (MatchedShelfDesign::designHigh (2500.0, 7.5, fs))));
    CHECK (equal (BandDesign::design (settings (FilterType::notch), fs),
                  single (MatchedNotchDesign::design (2500.0, 1.8, fs))));
    CHECK (equal (BandDesign::design (settings (FilterType::bandPass), fs),
                  single (MatchedBandpassDesign::design (2500.0, 1.8, fs))));
    CHECK (equal (BandDesign::design (settings (FilterType::allPass), fs),
                  single (MatchedAllpassDesign::design (2500.0, 1.8, fs))));
    CHECK (equal (BandDesign::design (settings (FilterType::tiltShelf), fs),
                  single (TiltShelfDesign::design (2500.0, 7.5, fs))));
    CHECK (equal (BandDesign::design (settings (FilterType::flatTilt), fs),
                  FlatTiltDesign::design (2500.0, 7.5, fs)));

    for (int slope = 0; slope < CutSlope::count; ++slope)
    {
        INFO ("slope index=" << slope);
        CHECK (equal (BandDesign::design (settings (FilterType::lowCut, 2500.0, 7.5, 1.8, slope), fs),
                      ButterworthCascade::design (Kind::lowCut, 2500.0, CutSlope::order (slope), fs)));
        CHECK (equal (BandDesign::design (settings (FilterType::highCut, 2500.0, 7.5, 1.8, slope), fs),
                      ButterworthCascade::design (Kind::highCut, 2500.0, CutSlope::order (slope), fs)));
    }
}

TEST_CASE ("Brickwall uses all sixteen sections", "[banddesign]")
{
    CHECK (BandDesign::design (settings (FilterType::highCut, 5000.0, 0.0, 0.71, CutSlope::brickwallIndex), fs).numSections == 16);
}

TEST_CASE ("A disabled band has no sections", "[banddesign]")
{
    for (int t = 0; t < FilterTypes::count; ++t)
    {
        auto s = settings (static_cast<FilterType> (t));
        s.enabled = false;
        INFO ("type=" << t);
        CHECK (BandDesign::design (s, fs).numSections == 0);
    }
}

TEST_CASE ("BandDesign keeps the frequency below Nyquist", "[banddesign]")
{
    // A 20 kHz band at 22.05 kHz sample rate would sit above Nyquist; it is capped at 0.49 fs.
    constexpr double lowRate = 22050.0;

    CHECK (equal (BandDesign::design (settings (FilterType::bell, 20000.0), lowRate),
                  BandDesign::design (settings (FilterType::bell, BandDesign::maxFrequencyRatio * lowRate), lowRate)));
    CHECK (BandDesign::design (settings (FilterType::bell, 20000.0), lowRate).isStable());
}
