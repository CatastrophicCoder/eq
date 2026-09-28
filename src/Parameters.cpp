#include "Parameters.h"

#include "dsp/CutSlope.h"

#include <cmath>

namespace
{
    /** Two decimals, and never "-0.00" (0 dB snaps to -6.7e-7 dB with a 0.01 dB interval in float). */
    juce::String formatDb (float v)
    {
        auto rounded = std::round (v * 100.0f) / 100.0f;
        if (std::abs (rounded) < 0.005f)
            rounded = 0.0f;
        return juce::String (rounded, 2);
    }

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

    juce::StringArray channelNames;
    for (auto* name : ChannelModes::names)
        channelNames.add (name);

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
                             .withStringFromValueFunction ([] (float v, int) { return formatDb (v); })));

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

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id (band, "channel"), m6VersionHint }, name + "Channel",
            channelNames, static_cast<int> (ChannelMode::stereo)));

        // Dynamics (M7). Continuous ranges without snapping intervals (see output_gain).
        auto dbText = [] (float v, int) { return formatDb (v); };
        auto msText = [] (float v, int) { return juce::String (v, v < 10.0f ? 1 : 0); };

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id (band, "dyn"), m7VersionHint }, name + "Dynamic", false));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id (band, "dynmode"), m7VersionHint }, name + "Dynamic Mode",
            juce::StringArray { "Range", "Ratio" }, 0));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (band, "thresh"), m7VersionHint }, name + "Threshold",
            juce::NormalisableRange<float> (-60.0f, 0.0f), -20.0f,
            FloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (band, "range"), m7VersionHint }, name + "Range",
            juce::NormalisableRange<float> (-24.0f, 24.0f), -6.0f,
            FloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (band, "ratio"), m7VersionHint }, name + "Ratio",
            logRange (1.0f, 20.0f), 2.0f,
            FloatAttributes().withLabel (":1").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (band, "attack"), m7VersionHint }, name + "Attack",
            logRange (0.1f, 200.0f), 10.0f,
            FloatAttributes().withLabel ("ms").withStringFromValueFunction (msText)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (band, "release"), m7VersionHint }, name + "Release",
            logRange (5.0f, 2000.0f), 100.0f,
            FloatAttributes().withLabel ("ms").withStringFromValueFunction (msText)));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id (band, "detector"), m7VersionHint }, name + "Detector",
            juce::StringArray { "Peak", "RMS" }, 0));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id (band, "sidechain"), m7VersionHint }, name + "Side-chain", false));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputGain, m2VersionHint }, "Output Gain",
        // No snapping interval: a 0.01 step turns the 0 dB default into -6.7e-7 dB in float,
        // and the output stage must be exactly unity (bit-exact bypass) at its default.
        juce::NormalisableRange<float> (-30.0f, 30.0f), 0.0f,
        FloatAttributes().withLabel ("dB")
                         .withStringFromValueFunction ([] (float v, int) { return formatDb (v); })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { autoGain, m2VersionHint }, "Auto Gain", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { outputInvert, m2VersionHint }, "Phase Invert", false));

    return layout;
}

BandSettings Parameters::toBandSettings (float type, float frequencyHz, float gainDb, float q, float slope, float enabled,
                                         bool inUse, float channel) noexcept
{
    BandSettings s;
    s.type = static_cast<FilterType> (juce::jlimit (0, FilterTypes::count - 1, juce::roundToInt (type)));
    s.frequencyHz = frequencyHz;
    s.gainDb = gainDb;
    s.q = q;
    s.slopeIndex = juce::jlimit (0, CutSlope::count - 1, juce::roundToInt (slope));
    s.enabled = enabled >= 0.5f;
    s.inUse = inUse;
    s.channel = static_cast<ChannelMode> (juce::jlimit (0, ChannelModes::count - 1, juce::roundToInt (channel)));
    return s;
}

BandSettings::Dynamics Parameters::toDynamics (const std::array<float, std::size (dynamicFields)>& raw) noexcept
{
    BandSettings::Dynamics d;
    d.on = raw[0] >= 0.5f;
    d.mode = raw[1] >= 0.5f ? DynamicGainLaw::Mode::ratio : DynamicGainLaw::Mode::range;
    d.thresholdDb = raw[2];
    d.rangeDb = raw[3];
    d.ratio = raw[4];
    d.attackMs = raw[5];
    d.releaseMs = raw[6];
    d.detector = raw[7] >= 0.5f ? LevelDetector::Mode::rms : LevelDetector::Mode::peak;
    d.sidechain = raw[8] >= 0.5f;
    return d;
}
