#include "FactoryPresets.h"

#include <initializer_list>

namespace
{
    // Band helpers. Slope indices: 0 = 6 dB/oct, 1 = 12 dB/oct.
    Preset::Band lowCut (float f, int slope = 1)  { return { true, true, FilterType::lowCut, f, 0.0f, 0.71f, slope, ChannelMode::stereo }; }
    Preset::Band highCut (float f, int slope = 1) { return { true, true, FilterType::highCut, f, 0.0f, 0.71f, slope, ChannelMode::stereo }; }
    Preset::Band bell (float f, float gain, float q) { return { true, true, FilterType::bell, f, gain, q, 3, ChannelMode::stereo }; }
    Preset::Band highShelf (float f, float gain) { return { true, true, FilterType::highShelf, f, gain, 0.71f, 3, ChannelMode::stereo }; }

    Preset make (const char* name, const char* category, std::initializer_list<Preset::Band> bands)
    {
        Preset p;
        p.name = name;
        p.category = category;

        size_t i = 0;
        for (const auto& b : bands)
            p.bands[i++] = b;

        return p;
    }
}

const std::vector<Preset>& FactoryPresets::all()
{
    // Starting points (decision 2026-09-28): frequencies and cut/boost directions as widely published for
    // each source; amounts kept moderate. Where published advice disagrees, the region is left flat.
    static const std::vector<Preset> presets {
        make ("Lead Vocal", "Vocals",
              { lowCut (90), bell (350, -2.0f, 1.0f), bell (3000, 1.5f, 1.4f), highShelf (12000, 2.0f) }),
        make ("Male Vocal", "Vocals",
              { lowCut (100), bell (350, -2.0f, 1.0f), bell (3000, 1.5f, 1.4f), highShelf (12000, 2.0f) }),
        make ("Female Vocal", "Vocals",   // the lead-vocal regions moved up about half an octave
              { lowCut (120), bell (500, -2.0f, 1.0f), bell (4200, 1.5f, 1.4f), highShelf (12000, 2.0f) }),

        make ("Acoustic Guitar", "Guitars",
              { lowCut (80), bell (250, -2.5f, 1.0f), bell (1200, -1.5f, 2.0f), bell (3000, 1.5f, 0.8f), highShelf (10000, 1.5f) }),
        make ("Electric Clean", "Guitars",
              { lowCut (80), bell (250, -1.5f, 1.0f), bell (3000, 1.5f, 1.0f) }),
        make ("Electric Rhythm", "Guitars",
              { lowCut (80), bell (100, 1.0f, 1.0f), bell (300, -3.0f, 1.4f), bell (2500, -2.0f, 1.0f), highCut (12000) }),

        make ("Bass DI", "Bass",
              { lowCut (30), bell (90, 1.5f, 1.0f), bell (300, -1.5f, 1.0f), bell (900, 1.5f, 1.0f) }),

        make ("Kick", "Drums",
              { lowCut (35), bell (80, 2.0f, 1.4f), bell (250, -3.0f, 1.0f), bell (3500, 2.0f, 1.4f) }),
        make ("Snare", "Drums",
              { lowCut (80), bell (400, -2.0f, 1.4f), bell (5000, 1.5f, 1.0f) }),
        make ("Overheads", "Drums",
              { lowCut (150), bell (4000, -1.5f, 1.0f), bell (10000, 1.5f, 0.7f) }),

        make ("Mix Bus Polish", "Mix",   // mix-bus moves stay around 1 dB
              { lowCut (30, 0), bell (250, -1.0f, 0.7f), highShelf (12000, 1.0f) }) };

    return presets;
}

const juce::StringArray& FactoryPresets::categories()
{
    static const juce::StringArray names { "Vocals", "Guitars", "Bass", "Drums", "Mix" };
    return names;
}
