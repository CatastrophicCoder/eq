#include "NodeLayout.h"

#include <algorithm>

std::vector<NodeLayout::Node> NodeLayout::compute (std::span<const BandSettings> bands, const FrequencyAxis& axis)
{
    const auto plot = axis.getPlotArea();
    std::vector<Node> nodes;

    for (size_t i = 0; i < bands.size(); ++i)
    {
        const auto& b = bands[i];
        if (! b.inUse)
            continue;

        const auto usesGain = FilterTypes::usesGain (b.type);
        const auto f = std::clamp (b.frequencyHz, FrequencyAxis::minHz, FrequencyAxis::maxHz);
        const auto y = usesGain ? axis.yForDb (b.gainDb) : axis.yForDb (0.0);

        nodes.push_back ({ static_cast<int> (i) + 1,
                           { axis.xForFrequency (f), juce::jlimit (plot.getY(), plot.getBottom(), y) },
                           usesGain,
                           b.enabled });
    }

    return nodes;
}

int NodeLayout::bandAt (const std::vector<Node>& nodes, juce::Point<float> point, float radius)
{
    int best = 0;
    auto bestDistance = radius;

    for (const auto& n : nodes)
    {
        const auto d = n.position.getDistanceFrom (point);
        if (d <= bestDistance)
        {
            best = n.band;
            bestDistance = d;
        }
    }

    return best;
}

std::vector<int> NodeLayout::bandsIn (const std::vector<Node>& nodes, juce::Rectangle<float> area)
{
    std::vector<int> result;

    for (const auto& n : nodes)
        if (area.contains (n.position))
            result.push_back (n.band);

    std::sort (result.begin(), result.end());
    return result;
}
