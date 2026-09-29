#include "UndoHistory.h"

#include "PluginProcessor.h"

UndoHistory::UndoHistory (ParametricEQAudioProcessor& p) : processor (p) {}
UndoHistory::~UndoHistory() = default;
void UndoHistory::attach() {}
bool UndoHistory::undo() { return false; }
bool UndoHistory::redo() { return false; }
void UndoHistory::clear() {}
void UndoHistory::begin (const char*) {}
void UndoHistory::end() {}
void UndoHistory::parameterGestureChanged (int, bool) {}
UndoHistory::ScopedTransaction::ScopedTransaction (UndoHistory& h, const char*) : history (h) {}
UndoHistory::ScopedTransaction::~ScopedTransaction() = default;
UndoHistory::ScopedSuspend::ScopedSuspend (UndoHistory& h) : history (h) {}
UndoHistory::ScopedSuspend::~ScopedSuspend() = default;
