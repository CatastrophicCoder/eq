#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <deque>

class ParametricEQAudioProcessor;

//==============================================================================
/** Undo/redo of sound edits (M9b, decisions 2026-09-29).

    A step is recorded around the user's own edits: parameter gestures (hosts never
    send them during automation) and explicit transactions for edits that are not
    parameters (bands in use, phase mode). The step stores sound-only snapshots
    (SettingSnapshot::Scope::sound) from before and after; undo and redo apply them
    as host edits. Preset loads, A/B switches and session loads clear the history
    (they run suspended). Keeps maxSteps steps; not saved. Message thread only.
*/
class UndoHistory final : private juce::AudioProcessorParameter::Listener
{
public:
    static constexpr int maxSteps = 100;
    static constexpr double mergeSeconds = 1.0;   // transactions with the same merge key this close form one step

    explicit UndoHistory (ParametricEQAudioProcessor& processor);
    ~UndoHistory() override;

    /** Listens to the processor's parameters; call once they exist. */
    void attach();

    bool canUndo() const noexcept { return ! undoSteps.empty(); }
    bool canRedo() const noexcept { return ! redoSteps.empty(); }
    int getNumSteps() const noexcept { return static_cast<int> (undoSteps.size()); }
    bool undo();
    bool redo();
    void clear();

    /** Groups everything inside into one step (nests; the outermost one records). A merge key
        joins it with the previous step of the same key if that ended less than mergeSeconds ago. */
    class ScopedTransaction
    {
    public:
        explicit ScopedTransaction (UndoHistory& history, const juce::String& mergeKey = {});
        ~ScopedTransaction();
    private:
        UndoHistory& history;
        JUCE_DECLARE_NON_COPYABLE (ScopedTransaction)
    };

    /** Nothing inside is recorded (preset loads, A/B switches, session loads, undo itself). */
    class ScopedSuspend
    {
    public:
        explicit ScopedSuspend (UndoHistory& history);
        ~ScopedSuspend();
    private:
        UndoHistory& history;
        JUCE_DECLARE_NON_COPYABLE (ScopedSuspend)
    };

private:
    struct Step
    {
        juce::ValueTree before, after;
        juce::String mergeKey;
        double endedAt = 0.0;
    };

    void begin (const juce::String& mergeKey);
    void end();
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;

    ParametricEQAudioProcessor& processor;
    std::deque<Step> undoSteps, redoSteps;
    juce::ValueTree pendingBefore;
    juce::String pendingKey;
    int depth = 0, suspended = 0;
};
