#include "Preset.h"

// Not implemented yet.
std::unique_ptr<juce::XmlElement> Preset::toXml() const { return std::make_unique<juce::XmlElement> ("none"); }
std::optional<Preset> Preset::fromXml (const juce::XmlElement&) { return std::nullopt; }
bool Preset::hasSameSettingsAs (const Preset&) const noexcept { return false; }
