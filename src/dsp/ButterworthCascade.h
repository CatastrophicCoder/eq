#pragma once

#include "SectionCascade.h"

//==============================================================================
/** Butterworth low cut / high cut of any order up to 32, as a cascade of matched
    second-order sections (MatchedHighpassDesign / MatchedLowpassDesign) plus one
    first-order section (FirstOrderCutDesign) for odd orders.

    Section Qs follow the Butterworth pole angles from the negative real axis:
    (2k - 1) pi / (2n) for even n, k pi / n for odd n; Q = 1 / (2 cos angle).
*/
class ButterworthCascade
{
public:
    enum class Kind { lowCut, highCut };

    static constexpr int maxOrder = 2 * SectionCascade::maxSections;

    static SectionCascade design (Kind kind, double cutoffHz, int order, double sampleRate) noexcept;

    /** Q of second-order section k (1-based) of an order-n Butterworth. */
    static double sectionQ (int order, int k) noexcept;

    /** Analog Butterworth magnitude, 1 / sqrt(1 + x^(2n)). */
    static double analogMagnitudeDb (Kind kind, double frequencyHz, double cutoffHz, int order) noexcept;
};
