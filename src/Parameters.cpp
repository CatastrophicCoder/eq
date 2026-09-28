#include "Parameters.h"

#include "dsp/CutSlope.h"

#include <cmath>

namespace
{
    /** A range mapped logarithmically onto 0..1, so equal knob travel is an equal ratio. */
    juce::NormalisableRange<float> logRange (float start, float end)
    {
        return { start, end,
                 [] (float min, float max, float normalised) { return min * std::pow (max / min, normalised); },
                 [] (float min, float max, float value)      { return std::log (value / min) / std::log (max / min); },
                 nullptr };
    }
}

juce::String Parameters::id (int band, const char* field)
{
    return "band" + juce::String (band) + "_" + field;
}

Parameters::BandDefaults Parameters::defaultsFor (int band)
{
    // Per-type presets (decision 2026-09-28): all disabled, 0 dB, bells Q 1, others Q 0.71, cuts 24 dB/oct.
    constexpr int cut24 = 3;

    struct Preset { FilterType type; float frequencyHz; };
    static constexpr Preset presets[numBands] {
        { FilterType::lowCut, 30.0f },    { FilterType::lowShelf, 80.0f },
        { FilterType::bell, 120.0f },     { FilterType::bell, 180.0f },   { FilterType::bell, 280.0f },
        { FilterType::bell, 420.0f },     { FilterType::bell, 640.0f },   { FilterType::bell, 970.0f },
        { FilterType::bell, 1500.0f },    { FilterType::bell, 2200.0f },  { FilterType::bell, 3400.0f },
        { FilterType::bell, 5200.0f },    { FilterType::bell, 7900.0f },  { FilterType::bell, 12000.0f },
        { FilterType::highShelf, 10000.0f }, { FilterType::highCut, 18000.0f } };

    const auto& preset = presets[juce::jlimit (1, numBands, band) - 1];
    return { preset.type, preset.frequencyHz, 0.0f, preset.type == FilterType::bell ? 1.0f : 0.71f, cut24, false };
}

juce::AudioProcessorValueTreeState::ParameterLayout Parameters::createLayout()
{
    using FloatAttributes = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray typeNames, slopeLabels;
    for (auto* name : FilterTypes::names)
        typeNames.add (name);
    for (auto* label : CutSlope::labels)
        slopeLabels.add (label);

    for (int band = 1; band <= numBands; ++band)
    {
        const auto d = defaultsFor (band);
        const auto name = "Band " + juce::String (band) + " ";
        const auto hintForM1Field = band == 1 ? m1VersionHint : m2VersionHint;

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (band, "freq"), hintForM1Field }, name + "Frequency",
            logRange (20.0f, 20000.0f), d.frequencyHz,
            FloatAttributes().withLabel ("Hz")
                             .withStringFromValueFunction ([] (float v, int) { return juce::String (v, v < 1000.0f ? 1 : 0); })));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (band, "gain"), hintForM1Field }, name + "Gain",
            juce::NormalisableRange<float> (-30.0f, 30.0f, 0.01f), d.gainDb,
            FloatAttributes().withLabel ("dB")
                             .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); })));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (band, "q"), hintForM1Field }, name + "Q",
            logRange (0.1f, 18.0f), d.q,
            FloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); })));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id (band, "type"), m2VersionHint }, name + "Type",
            typeNames, static_cast<int> (d.type)));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id (band, "slope"), m2VersionHint }, name + "Slope",
            slopeLabels, d.slopeIndex));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id (band, "enabled"), m2VersionHint }, name + "Enabled", d.enabled));
    }

    return layout;
}
