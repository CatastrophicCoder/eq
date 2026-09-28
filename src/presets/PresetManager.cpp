#include "PresetManager.h"

#include "PluginProcessor.h"

// Not implemented yet.
PresetManager::PresetManager (ParametricEQAudioProcessor& p, juce::File folder) : processor (p), userFolder (std::move (folder)) {}
juce::File PresetManager::defaultUserFolder() { return {}; }
std::vector<PresetManager::Entry> PresetManager::getEntries() const { return {}; }
Preset PresetManager::capture (const juce::String&) const { return {}; }
void PresetManager::apply (const Preset&) {}
bool PresetManager::load (const Entry&) { return false; }
PresetManager::SaveResult PresetManager::saveUserPreset (const juce::String&, bool) { return SaveResult::failed; }
bool PresetManager::deleteUserPreset (const Entry&) { return false; }
void PresetManager::loadNext() {}
void PresetManager::loadPrevious() {}
juce::String PresetManager::getCurrentName() const { return {}; }
bool PresetManager::isCurrentFactory() const { return false; }
bool PresetManager::isModified() const { return false; }
juce::String PresetManager::toFileName (const juce::String&) { return {}; }
void PresetManager::restoreFromSession() {}
