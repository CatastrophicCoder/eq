#include "NodeDragController.h"

#include <algorithm>
#include <cmath>

void NodeDragController::begin (std::vector<BandStart> bands, juce::Point<float> start)
{
    starts = std::move (bands);
    origin = start;
    active = true;
}

void NodeDragController::end()
{
    starts.clear();
    active = false;
}

std::vector<NodeDragController::BandValues> NodeDragController::dragTo (juce::Point<float> position, bool fine,
                                                                        const FrequencyAxis& axis) const
{
    const auto scale = fine ? fineFactor : 1.0f;
    const auto moved = origin + (position - origin) * scale;

    // Same frequency ratio and gain change for every band as for the pointer.
    const auto ratio = axis.frequencyForX (moved.x) / axis.frequencyForX (origin.x);
    const auto gainChange = axis.dbForY (moved.y) - axis.dbForY (origin.y);

    std::vector<BandValues> result;
    result.reserve (starts.size());

    for (const auto& s : starts)
        result.push_back ({ s.band,
                            std::clamp (s.frequencyHz * ratio, minFrequency, maxFrequency),
                            s.usesGain ? std::clamp (s.gainDb + gainChange, minGain, maxGain) : s.gainDb });

    return result;
}

double NodeDragController::scaleQ (double q, double factor) noexcept
{
    return std::clamp (q * factor, minQ, maxQ);
}

double NodeDragController::wheelFactor (float deltaY) noexcept
{
    return std::pow (2.0, 2.0 * static_cast<double> (deltaY));
}
