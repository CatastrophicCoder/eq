#include "AbComparison.h"

#include "PluginProcessor.h"
#include "SettingSnapshot.h"

namespace
{
    const juce::Identifier slotTag { "Slot" };
    const juce::Identifier activeProperty { "active" };
}

AbComparison::AbComparison (ParametricEQAudioProcessor& p) : processor (p)
{
}

juce::ValueTree AbComparison::capture() const
{
    return SettingSnapshot::capture (processor, SettingSnapshot::Scope::everything);
}

void AbComparison::apply (const juce::ValueTree& snapshot)
{
    SettingSnapshot::apply (processor, snapshot);
}

void AbComparison::switchTo (Slot slot)
{
    if (slot == active)
        return;

    // Not an undo step, and it starts a fresh undo history (decisions 2026-09-29).
    auto& undo = processor.getUndoHistory();
    {
        UndoHistory::ScopedSuspend suspend (undo);
        const auto current = capture();
        apply (other.isValid() ? other : current);   // a slot never set equals the setting it was split from
        other = current;
        active = slot;
    }
    undo.clear();
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
