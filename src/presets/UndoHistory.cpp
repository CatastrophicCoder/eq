#include "UndoHistory.h"

#include "PluginProcessor.h"
#include "SettingSnapshot.h"

UndoHistory::UndoHistory (ParametricEQAudioProcessor& p) : processor (p)
{
}

UndoHistory::~UndoHistory()
{
    for (auto* p : processor.getParameters())
        p->removeListener (this);
}

void UndoHistory::attach()
{
    for (auto* p : processor.getParameters())
        p->addListener (this);
}

void UndoHistory::parameterGestureChanged (int, bool gestureIsStarting)
{
    // Gestures come from the plugin's own UI (hosts do not send them during automation playback).
    if (! juce::MessageManager::existsAndIsCurrentThread())
        return;

    if (gestureIsStarting)
        begin ({});
    else
        end();
}

void UndoHistory::begin (const juce::String& mergeKey)
{
    if (suspended > 0)
        return;

    if (depth++ == 0)
    {
        pendingBefore = SettingSnapshot::capture (processor, SettingSnapshot::Scope::sound);
        pendingKey = mergeKey;
    }
}

void UndoHistory::end()
{
    if (suspended > 0 || depth == 0)
        return;

    if (--depth > 0)
        return;

    auto after = SettingSnapshot::capture (processor, SettingSnapshot::Scope::sound);
    if (after.isEquivalentTo (pendingBefore))
        return;   // nothing changed

    const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    if (pendingKey.isNotEmpty() && ! undoSteps.empty() && undoSteps.back().mergeKey == pendingKey
        && now - undoSteps.back().endedAt < mergeSeconds && redoSteps.empty())
    {
        undoSteps.back().after = after;
        undoSteps.back().endedAt = now;
        return;
    }

    undoSteps.push_back ({ pendingBefore, after, pendingKey, now });
    if (static_cast<int> (undoSteps.size()) > maxSteps)
        undoSteps.pop_front();
    redoSteps.clear();
}

bool UndoHistory::undo()
{
    if (undoSteps.empty())
        return false;

    auto step = undoSteps.back();
    undoSteps.pop_back();
    {
        ScopedSuspend suspend (*this);
        SettingSnapshot::apply (processor, step.before);
    }
    step.mergeKey = {};   // an undone step never merges again
    redoSteps.push_back (step);
    return true;
}

bool UndoHistory::redo()
{
    if (redoSteps.empty())
        return false;

    auto step = redoSteps.back();
    redoSteps.pop_back();
    {
        ScopedSuspend suspend (*this);
        SettingSnapshot::apply (processor, step.after);
    }
    undoSteps.push_back (step);
    return true;
}

void UndoHistory::clear()
{
    undoSteps.clear();
    redoSteps.clear();
}

UndoHistory::ScopedTransaction::ScopedTransaction (UndoHistory& h, const juce::String& mergeKey) : history (h)
{
    history.begin (mergeKey);
}

UndoHistory::ScopedTransaction::~ScopedTransaction()
{
    history.end();
}

UndoHistory::ScopedSuspend::ScopedSuspend (UndoHistory& h) : history (h)
{
    ++history.suspended;
}

UndoHistory::ScopedSuspend::~ScopedSuspend()
{
    --history.suspended;
}
