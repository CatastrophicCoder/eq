#include "FactoryPresets.h"

// Not implemented yet.
const std::vector<Preset>& FactoryPresets::all()
{
    static const std::vector<Preset> presets;
    return presets;
}

const juce::StringArray& FactoryPresets::categories()
{
    static const juce::StringArray names;
    return names;
}
