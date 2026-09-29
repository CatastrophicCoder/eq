#include "SettingSnapshot.h"

#include "Parameters.h"
#include "PluginProcessor.h"

namespace
{
    const juce::Identifier slotTag { "Slot" };
    const juce::Identifier linearPhaseKey { "_linearPhase" }, lengthKey { "_linearPhaseLength" }, rangeKey { "_displayRangeDb" };
    const juce::Identifier modeKey { "_analyzerMode" }, resolutionKey { "_analyzerResolution" }, speedKey { "_analyzerSpeed" };
    const juce::Identifier analyzerRangeKey { "_analyzerRange" }, presetKey { "_presetName" }, factoryKey { "_presetFactory" };

    juce::Identifier usedKey (int band) { return juce::Identifier ("_used" + juce::String (band)); }
}

juce::ValueTree SettingSnapshot::capture (const ParametricEQAudioProcessor& processor, Scope scope)
{
    juce::ValueTree snapshot (slotTag);

    // Parameters under their IDs; the other settings under keys starting with "_" (no parameter ID does).
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            snapshot.setProperty (juce::Identifier (ranged->getParameterID()), ranged->convertFrom0to1 (ranged->getValue()), nullptr);

    for (int band = 1; band <= Parameters::numBands; ++band)
        snapshot.setProperty (usedKey (band), processor.isBandInUse (band), nullptr);

    snapshot.setProperty (linearPhaseKey, processor.isLinearPhase(), nullptr);
    snapshot.setProperty (lengthKey, processor.getLinearPhaseLength(), nullptr);

    if (scope == Scope::everything)
    {
        const auto analyzer = processor.getAnalyzerSettings();
        snapshot.setProperty (rangeKey, processor.getDisplayRangeDb(), nullptr);
        snapshot.setProperty (modeKey, analyzer.mode, nullptr);
        snapshot.setProperty (resolutionKey, analyzer.resolution, nullptr);
        snapshot.setProperty (speedKey, analyzer.speed, nullptr);
        snapshot.setProperty (analyzerRangeKey, analyzer.range, nullptr);
        snapshot.setProperty (presetKey, processor.getStoredPresetName(), nullptr);
        snapshot.setProperty (factoryKey, processor.isStoredPresetFactory(), nullptr);
    }

    return snapshot;
}

void SettingSnapshot::apply (ParametricEQAudioProcessor& processor, const juce::ValueTree& snapshot)
{
    // Parameters first (as host edits, only where they differ), then the bands in use, so a band
    // that comes into use starts with its own settings.
    for (auto* p : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr)
            continue;

        const juce::Identifier id (ranged->getParameterID());
        if (! snapshot.hasProperty (id))
            continue;

        const auto normalised = ranged->convertTo0to1 (static_cast<float> (snapshot.getProperty (id)));
        if (std::abs (normalised - ranged->getValue()) <= 1.0e-7f)
            continue;

        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost (normalised);
        ranged->endChangeGesture();
    }

    for (int band = 1; band <= Parameters::numBands; ++band)
        if (snapshot.hasProperty (usedKey (band)))
        {
            const auto inUse = static_cast<bool> (snapshot.getProperty (usedKey (band)));
            if (inUse != processor.isBandInUse (band))
                processor.setBandInUse (band, inUse);
        }

    if (snapshot.hasProperty (lengthKey) && static_cast<int> (snapshot.getProperty (lengthKey)) != processor.getLinearPhaseLength())
        processor.setLinearPhaseLength (static_cast<int> (snapshot.getProperty (lengthKey)));
    if (snapshot.hasProperty (linearPhaseKey) && static_cast<bool> (snapshot.getProperty (linearPhaseKey)) != processor.isLinearPhase())
        processor.setLinearPhase (static_cast<bool> (snapshot.getProperty (linearPhaseKey)));

    if (snapshot.hasProperty (rangeKey))
        processor.setDisplayRangeDb (static_cast<double> (snapshot.getProperty (rangeKey)));

    if (snapshot.hasProperty (modeKey))
    {
        auto analyzer = processor.getAnalyzerSettings();
        analyzer.mode = static_cast<int> (snapshot.getProperty (modeKey, analyzer.mode));
        analyzer.resolution = static_cast<int> (snapshot.getProperty (resolutionKey, analyzer.resolution));
        analyzer.speed = static_cast<int> (snapshot.getProperty (speedKey, analyzer.speed));
        analyzer.range = static_cast<int> (snapshot.getProperty (analyzerRangeKey, analyzer.range));
        processor.setAnalyzerSettings (analyzer);
    }

    if (snapshot.hasProperty (presetKey))
    {
        processor.setStoredPreset (snapshot.getProperty (presetKey).toString(), static_cast<bool> (snapshot.getProperty (factoryKey, false)));
        processor.getPresetManager().restoreFromSession();
    }
}
