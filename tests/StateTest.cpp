#include "Parameters.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
    juce::RangedAudioParameter& param (ParametricEQAudioProcessor& p, const char* id)
    {
        auto* parameter = p.getValueTreeState().getParameter (id);
        REQUIRE (parameter != nullptr);
        return *parameter;
    }

    float value (ParametricEQAudioProcessor& p, const char* id)
    {
        return p.getValueTreeState().getRawParameterValue (id)->load();
    }

    void set (ParametricEQAudioProcessor& p, const char* id, float newValue)
    {
        auto& parameter = param (p, id);
        parameter.setValueNotifyingHost (parameter.convertTo0to1 (newValue));
    }

    void checkDefaults (ParametricEQAudioProcessor& p)
    {
        CHECK_THAT (value (p, Parameters::band1Freq), WithinRel (1000.0f, 1e-4f));
        CHECK_THAT (value (p, Parameters::band1Gain), WithinAbs (0.0f, 1e-6f));
        CHECK_THAT (value (p, Parameters::band1Q), WithinRel (0.71f, 1e-4f));
    }
}

TEST_CASE ("Band 1 parameters exist with the planned IDs, ranges and defaults", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    CHECK (juce::String (Parameters::band1Freq) == "band1_freq");
    CHECK (juce::String (Parameters::band1Gain) == "band1_gain");
    CHECK (juce::String (Parameters::band1Q) == "band1_q");

    const auto& freq = param (p, Parameters::band1Freq).getNormalisableRange();
    const auto& gain = param (p, Parameters::band1Gain).getNormalisableRange();
    const auto& q    = param (p, Parameters::band1Q).getNormalisableRange();

    CHECK_THAT (freq.start, WithinRel (20.0f));
    CHECK_THAT (freq.end,   WithinRel (20000.0f));
    CHECK_THAT (gain.start, WithinRel (-30.0f));
    CHECK_THAT (gain.end,   WithinRel (30.0f));
    CHECK_THAT (q.start,    WithinRel (0.1f));
    CHECK_THAT (q.end,      WithinRel (18.0f));

    // Frequency is log-scaled: the middle of the control is the geometric mean.
    CHECK_THAT (freq.convertFrom0to1 (0.5f), WithinRel (std::sqrt (20.0f * 20000.0f), 1e-4f));

    checkDefaults (p);
}

TEST_CASE ("State survives a save and load into a fresh processor", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    juce::MemoryBlock saved;
    {
        ParametricEQAudioProcessor source;
        set (source, Parameters::band1Freq, 3150.0f);
        set (source, Parameters::band1Gain, -4.5f);
        set (source, Parameters::band1Q, 2.2f);
        source.getStateInformation (saved);
    }

    ParametricEQAudioProcessor target;
    target.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));

    CHECK_THAT (value (target, Parameters::band1Freq), WithinRel (3150.0f, 1e-3f));
    CHECK_THAT (value (target, Parameters::band1Gain), WithinAbs (-4.5f, 0.01f));
    CHECK_THAT (value (target, Parameters::band1Q), WithinRel (2.2f, 1e-3f));
}

TEST_CASE ("Saved state carries a version number", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;

    juce::MemoryBlock saved;
    p.getStateInformation (saved);

    const auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), static_cast<int> (saved.getSize()));
    REQUIRE (xml != nullptr);
    CHECK (xml->getIntAttribute ("stateVersion", -1) == ParametricEQAudioProcessor::stateVersion);
    CHECK (ParametricEQAudioProcessor::stateVersion == 1);
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
        child->setAttribute ("id", Parameters::band1Gain);
        child->setAttribute ("value", 12.0);

        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (xml, block);
        p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    }

    SECTION ("wrong tag")
    {
        juce::XmlElement xml ("SomethingElse");
        xml.setAttribute ("stateVersion", 1);

        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (xml, block);
        p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    }

    checkDefaults (p);
}

TEST_CASE ("Processor applies the band 1 parameters to the audio", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 480;

    ParametricEQAudioProcessor p;
    set (p, Parameters::band1Freq, 1000.0f);
    set (p, Parameters::band1Gain, 12.0f);
    set (p, Parameters::band1Q, 1.0f);

    p.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);

    // 0.5 s of a 1 kHz sine at 0.1; measure the last 100 ms, well after any ramp.
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    float peak = 0.0f;

    for (int block = 0; block < 50; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                buffer.setSample (ch, i, static_cast<float> (0.1 * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0
                                                                            * (block * blockSize + i) / sampleRate)));

        p.processBlock (buffer, midi);

        if (block >= 40)
            peak = std::max (peak, buffer.getMagnitude (0, blockSize));
    }

    CHECK_THAT (juce::Decibels::gainToDecibels (peak / 0.1f), WithinAbs (12.0, 0.1));
}
