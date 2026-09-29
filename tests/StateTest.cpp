#include "TestParameters.h"

#include "dsp/CutSlope.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace TestParameters;

namespace
{
    void checkDefaults (ParametricEQAudioProcessor& p)
    {
        for (int band = 1; band <= Parameters::numBands; ++band)
        {
            const auto d = Parameters::defaultsFor (band);
            INFO ("band=" << band);
            CHECK_THAT (value (p, Parameters::id (band, "freq")), WithinRel (d.frequencyHz, 1e-4f));
            CHECK_THAT (value (p, Parameters::id (band, "gain")), WithinAbs (d.gainDb, 1e-6f));
            CHECK_THAT (value (p, Parameters::id (band, "type")), WithinAbs (static_cast<float> (d.type), 0.0f));
            CHECK_THAT (value (p, Parameters::id (band, "enabled")), WithinAbs (d.enabled ? 1.0f : 0.0f, 0.0f));
        }
    }

    juce::MemoryBlock toBlock (const juce::XmlElement& xml)
    {
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (xml, block);
        return block;
    }
}

TEST_CASE ("State survives a save and load into a fresh processor", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::Random random (2026);

    struct Values { int type; float freq, gain, q; int slope; bool enabled; };
    std::vector<Values> written;

    juce::MemoryBlock saved;
    {
        ParametricEQAudioProcessor source;

        for (int band = 1; band <= Parameters::numBands; ++band)
        {
            Values v { random.nextInt (FilterTypes::count), 20.0f * std::pow (1000.0f, random.nextFloat()),
                       -30.0f + 60.0f * random.nextFloat(), 0.1f * std::pow (180.0f, random.nextFloat()),
                       random.nextInt (CutSlope::count), random.nextBool() };
            setBand (source, band, static_cast<FilterType> (v.type), v.freq, v.gain, v.q, v.slope, v.enabled);
            written.push_back (v);
        }

        source.getStateInformation (saved);
    }

    ParametricEQAudioProcessor target;
    target.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        const auto& v = written[static_cast<size_t> (band - 1)];
        INFO ("band=" << band);
        CHECK_THAT (value (target, Parameters::id (band, "type")), WithinAbs (static_cast<float> (v.type), 0.0f));
        CHECK_THAT (value (target, Parameters::id (band, "freq")), WithinRel (v.freq, 1e-3f));
        CHECK_THAT (value (target, Parameters::id (band, "gain")), WithinAbs (v.gain, 0.01f));
        CHECK_THAT (value (target, Parameters::id (band, "q")), WithinRel (v.q, 1e-3f));
        CHECK_THAT (value (target, Parameters::id (band, "slope")), WithinAbs (static_cast<float> (v.slope), 0.0f));
        CHECK_THAT (value (target, Parameters::id (band, "enabled")), WithinAbs (v.enabled ? 1.0f : 0.0f, 0.0f));
    }
}

TEST_CASE ("Saved state carries version 3", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    juce::MemoryBlock saved;
    p.getStateInformation (saved);

    const auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), static_cast<int> (saved.getSize()));
    REQUIRE (xml != nullptr);
    CHECK (xml->getIntAttribute ("stateVersion", -1) == 5);
    CHECK (ParametricEQAudioProcessor::stateVersion == 5);
}

TEST_CASE ("A version-1 session (M1) loads as an enabled bell on band 1", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    // Exactly what M1's getStateInformation wrote: the APVTS tree with three PARAM children.
    juce::XmlElement v1 ("ParametricEQ");
    v1.setAttribute ("stateVersion", 1);
    for (auto [id, v] : { std::pair { "band1_freq", 3150.0 }, { "band1_gain", -4.5 }, { "band1_q", 2.2 } })
    {
        auto* child = v1.createNewChildElement ("PARAM");
        child->setAttribute ("id", id);
        child->setAttribute ("value", v);
    }

    const auto block = toBlock (v1);
    ParametricEQAudioProcessor p;
    p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));

    CHECK_THAT (value (p, "band1_freq"), WithinRel (3150.0f, 1e-3f));
    CHECK_THAT (value (p, "band1_gain"), WithinAbs (-4.5f, 0.01f));
    CHECK_THAT (value (p, "band1_q"), WithinRel (2.2f, 1e-3f));
    CHECK_THAT (value (p, "band1_type"), WithinAbs (static_cast<float> (FilterType::bell), 0.0f));
    CHECK_THAT (value (p, "band1_enabled"), WithinAbs (1.0f, 0.0f));
    CHECK (p.isBandInUse (1));
    for (int band = 2; band <= Parameters::numBands; ++band)
        CHECK_FALSE (p.isBandInUse (band));

    // Bands 2-16 keep their defaults.
    for (int band = 2; band <= Parameters::numBands; ++band)
    {
        const auto d = Parameters::defaultsFor (band);
        INFO ("band=" << band);
        CHECK_THAT (value (p, Parameters::id (band, "enabled")), WithinAbs (0.0f, 0.0f));
        CHECK_THAT (value (p, Parameters::id (band, "freq")), WithinRel (d.frequencyHz, 1e-4f));
    }
}

TEST_CASE ("Invalid state is ignored and leaves the defaults", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    SECTION ("empty block")
    {
        p.setStateInformation (nullptr, 0);
    }

    SECTION ("random bytes")
    {
        juce::Random random (1234);
        juce::MemoryBlock garbage (4096);
        random.fillBitsRandomly (garbage.getData(), garbage.getSize());
        p.setStateInformation (garbage.getData(), static_cast<int> (garbage.getSize()));
    }

    SECTION ("right tag but no version")
    {
        juce::XmlElement xml (p.getValueTreeState().state.getType());
        auto* child = xml.createNewChildElement ("PARAM");
        child->setAttribute ("id", "band1_gain");
        child->setAttribute ("value", 12.0);
        const auto block = toBlock (xml);
        p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    }

    SECTION ("a newer version than we know")
    {
        juce::XmlElement xml (p.getValueTreeState().state.getType());
        xml.setAttribute ("stateVersion", ParametricEQAudioProcessor::stateVersion + 1);
        auto* child = xml.createNewChildElement ("PARAM");
        child->setAttribute ("id", "band1_gain");
        child->setAttribute ("value", 12.0);
        const auto block = toBlock (xml);
        p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    }

    SECTION ("wrong tag")
    {
        juce::XmlElement xml ("SomethingElse");
        xml.setAttribute ("stateVersion", 2);
        const auto block = toBlock (xml);
        p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    }

    checkDefaults (p);
}

TEST_CASE ("A version-2 state from before the output parameters loads their defaults", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    juce::XmlElement v2 ("ParametricEQ");
    v2.setAttribute ("stateVersion", 2);
    auto* child = v2.createNewChildElement ("PARAM");
    child->setAttribute ("id", "band4_gain");
    child->setAttribute ("value", 5.0);

    const auto block = toBlock (v2);
    ParametricEQAudioProcessor p;
    set (p, Parameters::outputGain, 9.0f);   // must be reset to its default by the load
    p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));

    CHECK_THAT (value (p, "band4_gain"), WithinAbs (5.0f, 0.01f));
    CHECK_THAT (value (p, Parameters::outputGain), WithinAbs (0.0f, 1e-6f));
    CHECK_THAT (value (p, Parameters::autoGain), WithinAbs (0.0f, 0.0f));
    CHECK_THAT (value (p, Parameters::outputInvert), WithinAbs (0.0f, 0.0f));
}

TEST_CASE ("The display range is saved with the session", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    CHECK_THAT (ParametricEQAudioProcessor().getDisplayRangeDb(), WithinAbs (12.0, 0.0));

    juce::MemoryBlock saved;
    {
        ParametricEQAudioProcessor source;
        source.setDisplayRangeDb (30.0);
        CHECK_THAT (source.getDisplayRangeDb(), WithinAbs (30.0, 0.0));
        source.getStateInformation (saved);
    }

    ParametricEQAudioProcessor target;
    target.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    CHECK_THAT (target.getDisplayRangeDb(), WithinAbs (30.0, 0.0));
}

TEST_CASE ("A missing or invalid display range falls back to 12 dB", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    p.setDisplayRangeDb (7.0);   // not one of 3 / 6 / 12 / 30: ignored
    CHECK_THAT (p.getDisplayRangeDb(), WithinAbs (12.0, 0.0));

    for (auto stored : { juce::var(), juce::var (25.0), juce::var ("twelve") })
    {
        juce::XmlElement xml ("ParametricEQ");
        xml.setAttribute ("stateVersion", 2);
        if (! stored.isVoid())
            xml.setAttribute ("displayRangeDb", stored.toString());

        const auto block = toBlock (xml);
        p.setDisplayRangeDb (6.0);
        p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
        INFO ("stored " << stored.toString());
        CHECK_THAT (p.getDisplayRangeDb(), WithinAbs (12.0, 0.0));
    }
}

TEST_CASE ("A new instance has no band in use", "[state][bandstate]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    for (int band = 1; band <= Parameters::numBands; ++band)
        CHECK_FALSE (p.isBandInUse (band));
}

TEST_CASE ("In-use flags are saved with the session, separately from enabled", "[state][bandstate]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    juce::MemoryBlock saved;
    {
        ParametricEQAudioProcessor source;
        setBand (source, 3, FilterType::bell, 300.0f, 3.0f, 1.0f, 3, true);    // in use, enabled
        setBand (source, 7, FilterType::bell, 700.0f, 3.0f, 1.0f, 3, false);   // in use, disabled
        set (source, Parameters::id (9, "enabled"), 1.0f);                     // enabled but free
        source.getStateInformation (saved);
    }

    ParametricEQAudioProcessor target;
    target.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        INFO ("band " << band);
        CHECK (target.isBandInUse (band) == (band == 3 || band == 7));
    }

    CHECK_THAT (value (target, "band7_enabled"), WithinAbs (0.0f, 0.0f));
    CHECK_THAT (value (target, "band9_enabled"), WithinAbs (1.0f, 0.0f));
}

TEST_CASE ("A version-2 session marks its enabled bands as in use", "[state][bandstate]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    juce::XmlElement v2 ("ParametricEQ");
    v2.setAttribute ("stateVersion", 2);
    for (auto [id, v] : { std::pair { "band2_enabled", 1.0 }, { "band5_enabled", 0.0 }, { "band11_enabled", 1.0 } })
    {
        auto* child = v2.createNewChildElement ("PARAM");
        child->setAttribute ("id", id);
        child->setAttribute ("value", v);
    }

    const auto block = toBlock (v2);
    ParametricEQAudioProcessor p;
    p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        INFO ("band " << band);
        CHECK (p.isBandInUse (band) == (band == 2 || band == 11));
    }
}

TEST_CASE ("A version-3 session without an in-use flag leaves that band free", "[state][bandstate]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    juce::XmlElement v3 ("ParametricEQ");
    v3.setAttribute ("stateVersion", 3);
    v3.setAttribute ("band4_used", 1);
    auto* child = v3.createNewChildElement ("PARAM");
    child->setAttribute ("id", "band6_enabled");
    child->setAttribute ("value", 1.0);

    const auto block = toBlock (v3);
    ParametricEQAudioProcessor p;
    p.setBandInUse (8, true);   // must be replaced by the load
    p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        INFO ("band " << band);
        CHECK (p.isBandInUse (band) == (band == 4));
    }
}
