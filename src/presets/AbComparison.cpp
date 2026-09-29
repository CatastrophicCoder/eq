#include "AbComparison.h"

#include "Parameters.h"
#include "PluginProcessor.h"

namespace
{
    const juce::Identifier slotTag { "Slot" };
    const juce::Identifier activeProperty { "active" };
    const juce::Identifier linearPhaseKey { "_linearPhase" }, lengthKey { "_linearPhaseLength" }, rangeKey { "_displayRangeDb" };
    const juce::Identifier modeKey { "_analyzerMode" }, resolutionKey { "_analyzerResolution" }, speedKey { "_analyzerSpeed" };
    const juce::Identifier analyzerRangeKey { "_analyzerRange" }, presetKey { "_presetName" }, factoryKey { "_presetFactory" };

    juce::Identifier usedKey (int band) { return juce::Identifier ("_used" + juce::String (band)); }
}

AbComparison::AbComparison (ParametricEQAudioProcessor& p) : processor (p)
{
}

juce::ValueTree AbComparison::capture() const
{
    juce::ValueTree snapshot (slotTag);

    // Parameters under their IDs; the other settings under keys starting with "_" (no parameter ID does).
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            snapshot.setProperty (juce::Identifier (ranged->getParameterID()), ranged->convertFrom0to1 (ranged->getValue()), nullptr);

    for (int band = 1; band <= Parameters::numBands; ++band)
        snapshot.setProperty (usedKey (band), processor.isBandInUse (band), nullptr);

    const auto analyzer = processor.getAnalyzerSettings();
    snapshot.setProperty (linearPhaseKey, processor.isLinearPhase(), nullptr);
    snapshot.setProperty (lengthKey, processor.getLinearPhaseLength(), nullptr);
    snapshot.setProperty (rangeKey, processor.getDisplayRangeDb(), nullptr);
    snapshot.setProperty (modeKey, analyzer.mode, nullptr);
    snapshot.setProperty (resolutionKey, analyzer.resolution, nullptr);
    snapshot.setProperty (speedKey, analyzer.speed, nullptr);
    snapshot.setProperty (analyzerRangeKey, analyzer.range, nullptr);
    snapshot.setProperty (presetKey, processor.getStoredPresetName(), nullptr);
    snapshot.setProperty (factoryKey, processor.isStoredPresetFactory(), nullptr);
    return snapshot;
}

void AbComparison::apply (const juce::ValueTree& snapshot)
{
    // Parameters first (as host edits, only where they differ), then the bands in use, so a band
    // that comes into use starts with its own settings.
    for (auto* p : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        const juce::Identifier id (ranged != nullptr ? ranged->getParameterID() : juce::String ("-"));
        if (ranged == nullptr || ! snapshot.hasProperty (id))
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
            processor.setBandInUse (band, static_cast<bool> (snapshot.getProperty (usedKey (band))));

    processor.setLinearPhaseLength (static_cast<int> (snapshot.getProperty (lengthKey, 0)));
    processor.setLinearPhase (static_cast<bool> (snapshot.getProperty (linearPhaseKey, false)));
    processor.setDisplayRangeDb (static_cast<double> (snapshot.getProperty (rangeKey, processor.getDisplayRangeDb())));

    auto analyzer = processor.getAnalyzerSettings();
    analyzer.mode = static_cast<int> (snapshot.getProperty (modeKey, analyzer.mode));
    analyzer.resolution = static_cast<int> (snapshot.getProperty (resolutionKey, analyzer.resolution));
    analyzer.speed = static_cast<int> (snapshot.getProperty (speedKey, analyzer.speed));
    analyzer.range = static_cast<int> (snapshot.getProperty (analyzerRangeKey, analyzer.range));
    processor.setAnalyzerSettings (analyzer);

    processor.setStoredPreset (snapshot.getProperty (presetKey).toString(), static_cast<bool> (snapshot.getProperty (factoryKey, false)));
    processor.getPresetManager().restoreFromSession();
}

void AbComparison::switchTo (Slot slot)
{
    if (slot == active)
        return;

    const auto current = capture();
    apply (other.isValid() ? other : current);   // a slot never set equals the setting it was split from
    other = current;
    active = slot;
}

void AbComparison::copyActiveToOther()
{
    other = capture();
}

juce::ValueTree AbComparison::toState() const
{
    juce::ValueTree state (stateTag);
    state.setProperty (activeProperty, active == Slot::a ? "A" : "B", nullptr);
    state.appendChild (other.isValid() ? other.createCopy() : capture(), nullptr);
    return state;
}

void AbComparison::fromState (const juce::ValueTree& state)
{
    const auto slot = state.isValid() ? state.getChildWithName (slotTag) : juce::ValueTree();

    if (! slot.isValid())
    {
        active = Slot::a;
        other = {};
        return;
    }

    active = state.getProperty (activeProperty).toString() == "B" ? Slot::b : Slot::a;
    other = slot.createCopy();
}
