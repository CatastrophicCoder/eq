#include "TestParameters.h"

#include "dsp/CutSlope.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace TestParameters;

namespace
{
    const char* const fields[] { "freq", "gain", "q", "type", "slope", "enabled" };
}

TEST_CASE ("Parameter IDs carry the band index", "[parameters]")
{
    CHECK (Parameters::id (1, "freq") == "band1_freq");
    CHECK (Parameters::id (16, "enabled") == "band16_enabled");
    CHECK (Parameters::numBands == 16);
}

TEST_CASE ("Every band has all six parameters and nothing else exists yet", "[parameters]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    for (int band = 1; band <= Parameters::numBands; ++band)
        for (auto* field : fields)
        {
            INFO ("band=" << band << " field=" << field);
            CHECK (p.getValueTreeState().getParameter (Parameters::id (band, field)) != nullptr);
        }

    CHECK (p.getParameters().size() == 6 * Parameters::numBands);
}

TEST_CASE ("Band parameters have the planned ranges and choices", "[parameters]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        INFO ("band=" << band);

        const auto& freq = param (p, Parameters::id (band, "freq")).getNormalisableRange();
        CHECK_THAT (freq.start, WithinRel (20.0f));
        CHECK_THAT (freq.end, WithinRel (20000.0f));
        CHECK_THAT (freq.convertFrom0to1 (0.5f), WithinRel (std::sqrt (20.0f * 20000.0f), 1e-4f));

        const auto& gain = param (p, Parameters::id (band, "gain")).getNormalisableRange();
        CHECK_THAT (gain.start, WithinRel (-30.0f));
        CHECK_THAT (gain.end, WithinRel (30.0f));

        const auto& q = param (p, Parameters::id (band, "q")).getNormalisableRange();
        CHECK_THAT (q.start, WithinRel (0.1f));
        CHECK_THAT (q.end, WithinRel (18.0f));

        auto* type = dynamic_cast<juce::AudioParameterChoice*> (&param (p, Parameters::id (band, "type")));
        REQUIRE (type != nullptr);
        REQUIRE (type->choices.size() == FilterTypes::count);
        for (int i = 0; i < FilterTypes::count; ++i)
            CHECK (type->choices[i] == FilterTypes::names[i]);

        auto* slope = dynamic_cast<juce::AudioParameterChoice*> (&param (p, Parameters::id (band, "slope")));
        REQUIRE (slope != nullptr);
        REQUIRE (slope->choices.size() == CutSlope::count);
        for (int i = 0; i < CutSlope::count; ++i)
            CHECK (slope->choices[i] == CutSlope::labels[i]);

        CHECK (dynamic_cast<juce::AudioParameterBool*> (&param (p, Parameters::id (band, "enabled"))) != nullptr);
    }
}

TEST_CASE ("Band defaults are the agreed per-type presets, all disabled", "[parameters]")
{
    // Decision 2026-09-28. Bells Q 1, others Q 0.71; cuts 24 dB/oct (slope index 3).
    struct Expected { FilterType type; float freq; };
    const Expected expected[] {
        { FilterType::lowCut, 30.0f },   { FilterType::lowShelf, 80.0f },
        { FilterType::bell, 120.0f },    { FilterType::bell, 180.0f },   { FilterType::bell, 280.0f },
        { FilterType::bell, 420.0f },    { FilterType::bell, 640.0f },   { FilterType::bell, 970.0f },
        { FilterType::bell, 1500.0f },   { FilterType::bell, 2200.0f },  { FilterType::bell, 3400.0f },
        { FilterType::bell, 5200.0f },   { FilterType::bell, 7900.0f },  { FilterType::bell, 12000.0f },
        { FilterType::highShelf, 10000.0f }, { FilterType::highCut, 18000.0f } };

    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        const auto& e = expected[band - 1];
        const auto d = Parameters::defaultsFor (band);
        INFO ("band=" << band);

        CHECK (d.type == e.type);
        CHECK_THAT (d.frequencyHz, WithinRel (e.freq));
        CHECK_THAT (d.gainDb, WithinAbs (0.0f, 0.0f));
        CHECK_THAT (d.q, WithinRel (e.type == FilterType::bell ? 1.0f : 0.71f));
        CHECK (d.slopeIndex == 3);
        CHECK_FALSE (d.enabled);

        // And the live parameters start at those defaults.
        CHECK_THAT (value (p, Parameters::id (band, "type")), WithinAbs (static_cast<float> (e.type), 0.0f));
        CHECK_THAT (value (p, Parameters::id (band, "freq")), WithinRel (e.freq, 1e-4f));
        CHECK_THAT (value (p, Parameters::id (band, "gain")), WithinAbs (0.0f, 1e-6f));
        CHECK_THAT (value (p, Parameters::id (band, "q")), WithinRel (d.q, 1e-4f));
        CHECK_THAT (value (p, Parameters::id (band, "slope")), WithinAbs (3.0f, 0.0f));
        CHECK_THAT (value (p, Parameters::id (band, "enabled")), WithinAbs (0.0f, 0.0f));
    }
}

TEST_CASE ("Version hints: M1 parameters keep 1, M2 parameters are 2", "[parameters]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    for (int band = 1; band <= Parameters::numBands; ++band)
        for (auto* field : fields)
        {
            const auto isM1 = band == 1 && (juce::String (field) == "freq" || juce::String (field) == "gain"
                                            || juce::String (field) == "q");
            INFO ("band=" << band << " field=" << field);
            CHECK (param (p, Parameters::id (band, field)).getVersionHint() == (isM1 ? 1 : 2));
        }
}
