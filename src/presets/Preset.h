#pragma once

#include "dsp/BandSettings.h"
#include "dsp/ChannelMode.h"
#include "dsp/FilterType.h"

#include <juce_core/juce_core.h>

#include <array>
#include <memory>
#include <optional>

//==============================================================================
/** One preset: all 16 bands and the output section (decision 2026-09-28), with
    a versioned XML form for user preset files. View settings are not included.

    XML: <ParametricEQPreset formatVersion="1" name=".." category="..">, one
    <Band index=".." .../> per band in use (types and channel modes by name),
    output settings as attributes. Bands without an element are free.
*/
struct Preset
{
    static constexpr int formatVersion = 1;
    static constexpr const char* xmlTag = "ParametricEQPreset";

    struct Band
    {
        bool inUse = false;
        bool enabled = true;
        FilterType type = FilterType::bell;
        float frequencyHz = 1000.0f;
        float gainDb = 0.0f;
        float q = 0.71f;
        int slopeIndex = 3;
        ChannelMode channel = ChannelMode::stereo;
        BandSettings::Dynamics dynamics {};   // format 2 (M7); format 1 files load with dynamics off
    };

    juce::String name, category;
    std::array<Band, 16> bands {};
    float outputGainDb = 0.0f;
    bool autoGain = false;
    bool invert = false;

    std::unique_ptr<juce::XmlElement> toXml() const;

    /** Parses a preset; nullopt for a wrong tag, a missing or newer version, or no name.
        Values are clamped to the parameter ranges; bands with unknown types are skipped. */
    static std::optional<Preset> fromXml (const juce::XmlElement& xml);

    /** Same sound (bands and output), ignoring name and category; tolerant of float rounding. */
    bool hasSameSettingsAs (const Preset& other) const noexcept;
};
