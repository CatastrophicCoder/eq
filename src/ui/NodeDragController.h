#pragma once

#include "FrequencyAxis.h"

#include <vector>

//==============================================================================
/** Maths for dragging one or more nodes and for changing Q.

    A drag moves every band by the same frequency ratio and gain change as the
    pointer moved, from the values at the start of the drag; each result is
    clamped to its parameter range. Fine mode moves at fineFactor of the speed.
*/
class NodeDragController
{
public:
    static constexpr float fineFactor = 0.25f;
    static constexpr double minFrequency = 20.0, maxFrequency = 20000.0;
    static constexpr double minGain = -30.0, maxGain = 30.0;
    static constexpr double minQ = 0.1, maxQ = 18.0;

    struct BandStart  { int band; double frequencyHz, gainDb; bool usesGain; };
    struct BandValues { int band; double frequencyHz, gainDb; };

    void begin (std::vector<BandStart> bands, juce::Point<float> start);
    void end();
    bool isActive() const noexcept { return active; }
    const std::vector<BandStart>& getBands() const noexcept { return starts; }

    std::vector<BandValues> dragTo (juce::Point<float> position, bool fine, const FrequencyAxis& axis) const;

    /** Q multiplied by factor, clamped to minQ..maxQ. */
    static double scaleQ (double q, double factor) noexcept;

    /** Q factor for a wheel movement: 2^(2 deltaY), about x1.2 per mouse-wheel notch. */
    static double wheelFactor (float deltaY) noexcept;

private:
    std::vector<BandStart> starts;
    juce::Point<float> origin;
    bool active = false;
};
