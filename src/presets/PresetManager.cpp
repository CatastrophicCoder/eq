#include "PresetManager.h"

#include "FactoryPresets.h"
#include "Parameters.h"
#include "PluginProcessor.h"

#include <algorithm>

namespace
{
    /** One complete parameter edit with its host gesture. */
    void setWithGesture (juce::AudioProcessorValueTreeState& state, const juce::String& id, float value)
    {
        if (auto* p = state.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }

    std::optional<Preset> readFile (const juce::File& file)
    {
        if (const auto xml = juce::XmlDocument::parse (file))
            return Preset::fromXml (*xml);
        return std::nullopt;
    }
}

PresetManager::PresetManager (ParametricEQAudioProcessor& p, juce::File folder)
    : processor (p), userFolder (std::move (folder))
{
}

juce::File PresetManager::defaultUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/Catastrophic Audio/Spectral Fault");
}

juce::File PresetManager::legacyUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/CatastrophicCoder/ParametricEQ");
}

int PresetManager::copyLegacyPresets (const juce::File& legacy, const juce::File& target)
{
    if (! legacy.isDirectory() || ! target.findChildFiles (juce::File::findFiles, false, "*.xml").isEmpty())
        return 0;

    const auto files = legacy.findChildFiles (juce::File::findFiles, false, "*.xml");
    if (files.isEmpty() || ! target.createDirectory())
        return 0;

    int copied = 0;
    for (const auto& file : files)
        if (file.copyFileTo (target.getChildFile (file.getFileName())))
            ++copied;

    return copied;
}

void PresetManager::copyLegacyPresetsOnce() const
{
    // Only the real default folder migrates; tests use temporary folders (setUserFolder).
    if (legacyChecked)
        return;

    legacyChecked = true;
    if (userFolder == defaultUserFolder())
        copyLegacyPresets (legacyUserFolder(), userFolder);
}

std::vector<PresetManager::Entry> PresetManager::getEntries() const
{
    copyLegacyPresetsOnce();
    std::vector<Entry> entries;

    for (const auto& p : FactoryPresets::all())
        entries.push_back ({ p.name, p.category, true, {} });

    std::vector<Entry> user;
    for (const auto& file : userFolder.findChildFiles (juce::File::findFiles, false, "*.xml"))
        if (const auto preset = readFile (file))
            user.push_back ({ preset->name, "User", false, file });

    std::sort (user.begin(), user.end(), [] (const Entry& a, const Entry& b) { return a.name.compareIgnoreCase (b.name) < 0; });
    entries.insert (entries.end(), user.begin(), user.end());
    return entries;
}

Preset PresetManager::capture (const juce::String& name) const
{
    Preset p;
    p.name = name;
    p.category = "User";

    const auto bands = processor.getBandSettings();
    for (size_t i = 0; i < bands.size(); ++i)
    {
        const auto& s = bands[i];
        p.bands[i] = { s.inUse, s.enabled, s.type, static_cast<float> (s.frequencyHz), static_cast<float> (s.gainDb),
                       static_cast<float> (s.q), s.slopeIndex, s.channel, s.dynamics };
    }

    auto& state = processor.getValueTreeState();
    p.outputGainDb = state.getRawParameterValue (Parameters::outputGain)->load();
    p.autoGain = state.getRawParameterValue (Parameters::autoGain)->load() >= 0.5f;
    p.invert = state.getRawParameterValue (Parameters::outputInvert)->load() >= 0.5f;
    return p;
}

void PresetManager::apply (const Preset& preset)
{
    // A preset load is not an undo step and starts a fresh undo history (decisions 2026-09-29).
    UndoHistory::ScopedSuspend suspendUndo (processor.getUndoHistory());
    processor.getUndoHistory().clear();

    auto& state = processor.getValueTreeState();

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        const auto& b = preset.bands[static_cast<size_t> (band - 1)];

        if (! b.inUse)
        {
            processor.setBandInUse (band, false);
            continue;
        }

        // Settings first, so the band starts with the right filter once it is in use.
        setWithGesture (state, Parameters::id (band, "type"), static_cast<float> (b.type));
        setWithGesture (state, Parameters::id (band, "freq"), b.frequencyHz);
        setWithGesture (state, Parameters::id (band, "gain"), b.gainDb);
        setWithGesture (state, Parameters::id (band, "q"), b.q);
        setWithGesture (state, Parameters::id (band, "slope"), static_cast<float> (b.slopeIndex));
        setWithGesture (state, Parameters::id (band, "channel"), static_cast<float> (b.channel));

        const auto& d = b.dynamics;
        const float dynamics[] { d.on ? 1.0f : 0.0f, d.mode == DynamicGainLaw::Mode::ratio ? 1.0f : 0.0f,
                                 static_cast<float> (d.thresholdDb), static_cast<float> (d.rangeDb), static_cast<float> (d.ratio),
                                 static_cast<float> (d.attackMs), static_cast<float> (d.releaseMs),
                                 d.detector == LevelDetector::Mode::rms ? 1.0f : 0.0f, d.sidechain ? 1.0f : 0.0f };
        static_assert (std::size (dynamics) == std::size (Parameters::dynamicFields));
        for (size_t i = 0; i < std::size (dynamics); ++i)
            setWithGesture (state, Parameters::id (band, Parameters::dynamicFields[i]), dynamics[i]);
        setWithGesture (state, Parameters::id (band, "enabled"), b.enabled ? 1.0f : 0.0f);
        processor.setBandInUse (band, true);
    }

    setWithGesture (state, Parameters::outputGain, preset.outputGainDb);
    setWithGesture (state, Parameters::autoGain, preset.autoGain ? 1.0f : 0.0f);
    setWithGesture (state, Parameters::outputInvert, preset.invert ? 1.0f : 0.0f);
}

bool PresetManager::load (const Entry& entry)
{
    std::optional<Preset> preset;

    if (entry.isFactory)
    {
        for (const auto& p : FactoryPresets::all())
            if (p.name == entry.name)
                preset = p;
    }
    else
    {
        preset = readFile (entry.file);
    }

    if (! preset.has_value())
        return false;

    apply (*preset);
    loaded = preset;
    processor.setStoredPreset (preset->name, entry.isFactory);
    return true;
}

PresetManager::SaveResult PresetManager::saveUserPreset (const juce::String& name, bool overwrite)
{
    copyLegacyPresetsOnce();   // before the first save makes the new folder non-empty
    const auto fileName = toFileName (name);
    if (fileName.isEmpty())
        return SaveResult::invalidName;

    const auto file = userFolder.getChildFile (fileName + ".xml");
    if (file.exists() && ! overwrite)
        return SaveResult::alreadyExists;

    if (! userFolder.createDirectory())
        return SaveResult::failed;

    const auto preset = capture (name.trim());
    if (! preset.toXml()->writeTo (file))
        return SaveResult::failed;

    loaded = preset;
    processor.setStoredPreset (preset.name, false);
    return SaveResult::saved;
}

bool PresetManager::deleteUserPreset (const Entry& entry)
{
    if (entry.isFactory || ! entry.file.existsAsFile() || ! entry.file.deleteFile())
        return false;

    if (! isCurrentFactory() && getCurrentName() == entry.name)
    {
        loaded.reset();
        processor.setStoredPreset ({}, false);
    }

    return true;
}

void PresetManager::loadNext()
{
    const auto entries = getEntries();
    if (entries.empty())
        return;

    auto current = -1;
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].name == getCurrentName() && entries[i].isFactory == isCurrentFactory())
            current = static_cast<int> (i);

    load (entries[static_cast<size_t> ((current + 1) % static_cast<int> (entries.size()))]);
}

void PresetManager::loadPrevious()
{
    const auto entries = getEntries();
    if (entries.empty())
        return;

    const auto count = static_cast<int> (entries.size());
    auto current = count;   // "none" steps back to the last one
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].name == getCurrentName() && entries[i].isFactory == isCurrentFactory())
            current = static_cast<int> (i);

    load (entries[static_cast<size_t> ((current - 1 + count) % count)]);
}

juce::String PresetManager::getCurrentName() const
{
    return processor.getStoredPresetName();
}

bool PresetManager::isCurrentFactory() const
{
    return processor.isStoredPresetFactory();
}

bool PresetManager::isModified() const
{
    return loaded.has_value() && ! capture ({}).hasSameSettingsAs (*loaded);
}

juce::String PresetManager::toFileName (const juce::String& name)
{
    juce::String result;

    for (auto c : name)
        if (c >= 32 && juce::String ("\\/:*?\"<>|").indexOfChar (c) < 0)
            result += c;

    return result.trim().trimCharactersAtStart (" .").trimCharactersAtEnd (" .");
}

void PresetManager::restoreFromSession()
{
    loaded.reset();
    const auto name = getCurrentName();
    if (name.isEmpty())
        return;

    for (const auto& entry : getEntries())
        if (entry.name == name && entry.isFactory == isCurrentFactory())
        {
            if (entry.isFactory)
            {
                for (const auto& p : FactoryPresets::all())
                    if (p.name == name)
                        loaded = p;
            }
            else
            {
                loaded = readFile (entry.file);
            }
        }
}
