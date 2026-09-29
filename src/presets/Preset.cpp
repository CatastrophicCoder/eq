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

    // Choice names as the parameters show them (Parameters.cpp).
    const char* const modeNames[] { "Range", "Ratio" };
    const char* const detectorNames[] { "Peak", "RMS" };

    /** Format 2 dynamics, clamped to the parameter ranges; defaults for anything missing or unknown. */
    BandSettings::Dynamics readDynamics (const juce::XmlElement* e, int version)
    {
        BandSettings::Dynamics d;
        if (e == nullptr)
            return d;

        d.on = e->getBoolAttribute ("on", d.on);
        d.mode = indexOf (modeNames, e->getStringAttribute ("mode")) == 1 ? DynamicGainLaw::Mode::ratio : DynamicGainLaw::Mode::range;
        d.thresholdDb = std::clamp (e->getDoubleAttribute ("threshold", d.thresholdDb), -60.0, 0.0);
        d.rangeDb = std::clamp (e->getDoubleAttribute ("range", d.rangeDb), -24.0, 24.0);
        d.ratio = std::clamp (e->getDoubleAttribute ("ratio", d.ratio), 1.0, 20.0);
        d.attackMs = std::clamp (e->getDoubleAttribute ("attack", d.attackMs), 0.1, 200.0);
        d.releaseMs = std::clamp (e->getDoubleAttribute ("release", d.releaseMs), 5.0, 2000.0);
        d.detector = indexOf (detectorNames, e->getStringAttribute ("detector")) == 1 ? LevelDetector::Mode::rms : LevelDetector::Mode::peak;
        d.sidechain = e->getBoolAttribute ("sidechain", d.sidechain);
        d.spectral = version >= 3 && e->getBoolAttribute ("spectral", false);
        return d;
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

        const auto& d = b.dynamics;
        auto* dyn = e->createNewChildElement ("Dynamics");
        dyn->setAttribute ("on", d.on);
        dyn->setAttribute ("mode", modeNames[d.mode == DynamicGainLaw::Mode::ratio ? 1 : 0]);
        dyn->setAttribute ("threshold", d.thresholdDb);
        dyn->setAttribute ("range", d.rangeDb);
        dyn->setAttribute ("ratio", d.ratio);
        dyn->setAttribute ("attack", d.attackMs);
        dyn->setAttribute ("release", d.releaseMs);
        dyn->setAttribute ("detector", detectorNames[d.detector == LevelDetector::Mode::rms ? 1 : 0]);
        dyn->setAttribute ("sidechain", d.sidechain);
        dyn->setAttribute ("spectral", d.spectral);
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
        b.dynamics = readDynamics (version >= 2 ? e->getChildByName ("Dynamics") : nullptr, version);
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

        const auto& da = a.dynamics;
        const auto& db = b.dynamics;
        auto f = [] (double v) { return static_cast<float> (v); };
        if (da.on != db.on || da.mode != db.mode || da.detector != db.detector || da.sidechain != db.sidechain
            || da.spectral != db.spectral
            || ! sameDb (f (da.thresholdDb), f (db.thresholdDb)) || ! sameDb (f (da.rangeDb), f (db.rangeDb))
            || ! sameRatio (f (da.ratio), f (db.ratio)) || ! sameRatio (f (da.attackMs), f (db.attackMs))
            || ! sameRatio (f (da.releaseMs), f (db.releaseMs)))
            return false;
    }

    return true;
}
