#pragma once

#include "FrequencyAxis.h"
#include "dsp/BandSettings.h"

#include <span>
#include <vector>

//==============================================================================
/** Where each enabled band's node sits on the display, and hit-testing.

    x follows the frequency; y follows the gain for types that have one, and
    sits on the 0 dB line otherwise (decision 2026-09-28). Nodes are held
    inside the plot area.
*/
class NodeLayout
{
public:
    static constexpr float hitRadius = 10.0f;

    struct Node
    {
        int band;                       // 1-based
        juce::Point<float> position;
        bool usesGain;
        bool enabled = true;            // false: in use but disabled (drawn grey, not editable)
        ChannelMode channel = ChannelMode::stereo;   // for the badge
    };

    static std::vector<Node> compute (std::span<const BandSettings> bands, const FrequencyAxis& axis);

    /** Nearest node within radius, or 0 if none. */
    static int bandAt (const std::vector<Node>& nodes, juce::Point<float> point, float radius = hitRadius);

    static std::vector<int> bandsIn (const std::vector<Node>& nodes, juce::Rectangle<float> area);
};
