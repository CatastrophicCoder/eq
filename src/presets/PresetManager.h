#pragma once

#include "Preset.h"

#include <juce_core/juce_core.h>

#include <vector>

class ParametricEQAudioProcessor;

//==============================================================================
/** Factory and user presets for one plugin instance. Message thread only: it
    reads and writes files and sets parameters (each inside a host gesture).

    User presets are XML files in the user folder, by default
    ~/Library/Audio/Presets/Catastrophic Audio/Spectral Fault/ (renamed in M7; presets in the
    old folder are copied over the first time the list is read).
    The current preset's name is stored in the session.
*/
class PresetManager
{
public:
    struct Entry
    {
        juce::String name, category;
        bool isFactory = true;
        juce::File file;   // user presets only
    };

    enum class SaveResult { saved, alreadyExists, invalidName, failed };

    PresetManager (ParametricEQAudioProcessor& processor, juce::File userFolder);

    static juce::File defaultUserFolder();

    /** The user folder before the rename (M7): ~/Library/Audio/Presets/CatastrophicCoder/ParametricEQ/. */
    static juce::File legacyUserFolder();

    /** Copies the *.xml files of the legacy folder into the target folder if the target
        has none yet; the legacy files stay. Returns the number of files copied.
    */
    static int copyLegacyPresets (const juce::File& legacy, const juce::File& target);
    const juce::File& getUserFolder() const noexcept { return userFolder; }

    /** Tests point this at a temporary folder, so they never touch the real user presets. */
    void setUserFolder (juce::File folder) { userFolder = std::move (folder); }

    /** Factory presets in their order, then user presets sorted by name (rescans the folder). */
    std::vector<Entry> getEntries() const;

    Preset capture (const juce::String& name) const;
    void apply (const Preset& preset);

    bool load (const Entry& entry);
    SaveResult saveUserPreset (const juce::String& name, bool overwrite);
    bool deleteUserPreset (const Entry& entry);

    /** Step through getEntries() from the current preset, wrapping at either end. */
    void loadNext();
    void loadPrevious();

    juce::String getCurrentName() const;
    bool isCurrentFactory() const;
    bool isModified() const;

    /** File-name form of a preset name: characters not allowed in file names removed; empty if nothing is left. */
    static juce::String toFileName (const juce::String& name);

    /** Re-reads the current preset after the processor loaded a session. */
    void restoreFromSession();

private:
    ParametricEQAudioProcessor& processor;
    juce::File userFolder;
    std::optional<Preset> loaded;
    mutable bool legacyChecked = false;

    void copyLegacyPresetsOnce() const;   // what the current preset contained when loaded or saved
};
