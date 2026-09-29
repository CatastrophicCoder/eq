#pragma once

#include "PhaseModeControls.h"

#include <juce_gui_basics/juce_gui_basics.h>

class PresetManager;

//==============================================================================
/** Thin bar above the display: the plugin name and the preset browser (M6b):
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

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showSaveDialog();

    PresetManager& presets;
    juce::TextButton previousButton { "<" }, nextButton { ">" }, presetButton;
    PhaseModeControls phaseModeControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
};
