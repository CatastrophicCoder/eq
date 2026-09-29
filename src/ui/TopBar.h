#pragma once

#include "PhaseModeControls.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

class PresetManager;

//==============================================================================
/** Thin bar above the display: the plugin name, the preset browser (M6b) and, on the
    right, the phase mode menus (M8). Preset browser:
    previous / next, the current preset's name (with "*" when modified), and a
    menu with factory presets by category, user presets, Save As and Delete.
*/
class TopBar final : public juce::Component
{
public:
    explicit TopBar (PresetManager& presets);

    /** Updates the name shown. Message thread only. */
    void refresh();

    juce::PopupMenu buildPresetMenu() const;
    void applyPresetMenuResult (int itemId);

    /** Save As with a given name (the dialog calls this; tests call it directly). */
    void saveAs (const juce::String& name);

    static constexpr int menuFactoryBase = 1;
    static constexpr int menuUserBase = 1000;
    static constexpr int menuSaveAs = 5000;
    static constexpr int menuDelete = 5001;

    juce::TextButton& getPresetButton() noexcept   { return presetButton; }
    juce::TextButton& getPreviousButton() noexcept { return previousButton; }
    juce::TextButton& getNextButton() noexcept     { return nextButton; }
    PhaseModeControls& getPhaseModeControls() noexcept { return phaseModeControls; }

    /** A/B (M9a): the editor connects them; showActive() updates the buttons without notifying. */
    juce::TextButton& getAButton() noexcept    { return aButton; }
    juce::TextButton& getBButton() noexcept    { return bButton; }
    juce::TextButton& getCopyButton() noexcept { return copyButton; }

    /** Undo/redo (M9b): the editor connects them and sets whether they are enabled. */
    juce::TextButton& getUndoButton() noexcept { return undoButton; }
    juce::TextButton& getRedoButton() noexcept { return redoButton; }
    std::function<void()> onUndo, onRedo;
    void showActiveSlot (bool bIsActive);
    std::function<void (bool b)> onSlotChosen;
    std::function<void()> onCopy;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showSaveDialog();

    PresetManager& presets;
    juce::TextButton previousButton { "<" }, nextButton { ">" }, presetButton;
    PhaseModeControls phaseModeControls;
    juce::TextButton aButton { "A" }, bButton { "B" }, copyButton;
    juce::TextButton undoButton, redoButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
};
