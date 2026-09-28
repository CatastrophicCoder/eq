#include "Preset.h"

#include "dsp/CutSlope.h"

#include <algorithm>
#include <cmath>

namespace
{
    template <size_t N>
    int indexOf (const char* const (&names)[N], const juce::String& name)
    {
        for (size_t i = 0; i < N; ++i)
            if (name == names[i])
                return static_cast<int> (i);
        return -1;
    }
}

std::unique_ptr<juce::XmlElement> Preset::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> (xmlTag);
    xml->setAttribute ("formatVersion", formatVersion);
    xml->setAttribute ("name", name);
    xml->setAttribute ("category", category);
    xml->setAttribute ("outputGain", static_cast<double> (outputGainDb));
    xml->setAttribute ("autoGain", autoGain);
    xml->setAttribute ("invert", invert);

    for (size_t i = 0; i < bands.size(); ++i)
    {
        const auto& b = bands[i];
        if (! b.inUse)
            continue;

        auto* e = xml->createNewChildElement ("Band");
        e->setAttribute ("index", static_cast<int> (i) + 1);
        e->setAttribute ("enabled", b.enabled);
        e->setAttribute ("type", FilterTypes::names[static_cast<int> (b.type)]);   // by name: readable, order-proof
        e->setAttribute ("freq", static_cast<double> (b.frequencyHz));
        e->setAttribute ("gain", static_cast<double> (b.gainDb));
        e->setAttribute ("q", static_cast<double> (b.q));
        e->setAttribute ("slope", b.slopeIndex);
        e->setAttribute ("channel", ChannelModes::names[static_cast<int> (b.channel)]);
    }

    return xml;
}

std::optional<Preset> Preset::fromXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (xmlTag) || ! xml.hasAttribute ("formatVersion"))
        return std::nullopt;

    const auto version = xml.getIntAttribute ("formatVersion");
    if (version < 1 || version > formatVersion)
        return std::nullopt;

    Preset p;
    p.name = xml.getStringAttribute ("name").trim();
    if (p.name.isEmpty())
        return std::nullopt;

    p.category = xml.getStringAttribute ("category");
    p.outputGainDb = std::clamp (static_cast<float> (xml.getDoubleAttribute ("outputGain")), -30.0f, 30.0f);
    p.autoGain = xml.getBoolAttribute ("autoGain");
    p.invert = xml.getBoolAttribute ("invert");

    for (auto* e : xml.getChildWithTagNameIterator ("Band"))
    {
        const auto index = e->getIntAttribute ("index");
        const auto type = indexOf (FilterTypes::names, e->getStringAttribute ("type"));
        if (index < 1 || index > static_cast<int> (p.bands.size()) || type < 0)
            continue;   // unknown band or type: leave the band free

        auto& b = p.bands[static_cast<size_t> (index - 1)];
        b.inUse = true;
        b.enabled = e->getBoolAttribute ("enabled", true);
        b.type = static_cast<FilterType> (type);
        b.frequencyHz = std::clamp (static_cast<float> (e->getDoubleAttribute ("freq", 1000.0)), 20.0f, 20000.0f);
        b.gainDb = std::clamp (static_cast<float> (e->getDoubleAttribute ("gain")), -30.0f, 30.0f);
        b.q = std::clamp (static_cast<float> (e->getDoubleAttribute ("q", 0.71)), 0.1f, 18.0f);
        b.slopeIndex = std::clamp (e->getIntAttribute ("slope", 3), 0, CutSlope::count - 1);
        b.channel = static_cast<ChannelMode> (std::max (0, indexOf (ChannelModes::names, e->getStringAttribute ("channel", "Stereo"))));
    }

    return p;
}

bool Preset::hasSameSettingsAs (const Preset& other) const noexcept
{
    auto sameRatio = [] (float a, float b) { return std::abs (a - b) <= 1.0e-3f * std::max (std::abs (a), std::abs (b)); };
    auto sameDb = [] (float a, float b) { return std::abs (a - b) <= 0.01f; };

    if (! sameDb (outputGainDb, other.outputGainDb) || autoGain != other.autoGain || invert != other.invert)
        return false;

    for (size_t i = 0; i < bands.size(); ++i)
    {
        const auto& a = bands[i];
        const auto& b = other.bands[i];

        if (a.inUse != b.inUse)
            return false;
        if (! a.inUse)
            continue;

        if (a.enabled != b.enabled || a.type != b.type || a.channel != b.channel || a.slopeIndex != b.slopeIndex
            || ! sameRatio (a.frequencyHz, b.frequencyHz) || ! sameDb (a.gainDb, b.gainDb) || ! sameRatio (a.q, b.q))
            return false;
    }

    return true;
}
