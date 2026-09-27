#include "ButterworthCascade.h"

#include "FirstOrderCutDesign.h"
#include "MatchedHighpassDesign.h"
#include "MatchedLowpassDesign.h"

#include <algorithm>
#include <cmath>
#include <numbers>

SectionCascade ButterworthCascade::design (Kind kind, double cutoffHz, int order, double sampleRate) noexcept
{
    SectionCascade cascade;
    order = std::clamp (order, 1, maxOrder);

    for (int k = 1; k <= order / 2; ++k)
    {
        const auto q = sectionQ (order, k);
        cascade.add (kind == Kind::highCut ? MatchedLowpassDesign::design (cutoffHz, q, sampleRate)
                                           : MatchedHighpassDesign::design (cutoffHz, q, sampleRate));
    }

    if (order % 2 != 0)
        cascade.add (kind == Kind::highCut ? FirstOrderCutDesign::designLowpass (cutoffHz, sampleRate)
                                           : FirstOrderCutDesign::designHighpass (cutoffHz, sampleRate));

    return cascade;
}

double ButterworthCascade::sectionQ (int order, int k) noexcept
{
    // Pole angle from the negative real axis.
    const auto angle = order % 2 == 0 ? (2.0 * k - 1.0) * std::numbers::pi / (2.0 * order)
                                      : k * std::numbers::pi / order;
    return 1.0 / (2.0 * std::cos (angle));
}

double ButterworthCascade::analogMagnitudeDb (Kind kind, double frequencyHz, double cutoffHz, int order) noexcept
{
    const auto x = kind == Kind::highCut ? frequencyHz / cutoffHz : cutoffHz / frequencyHz;
    return -10.0 * std::log10 (1.0 + std::pow (x, 2.0 * order));
}
