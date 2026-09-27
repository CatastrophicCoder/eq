#include "ButterworthCascade.h"

#include <cmath>

SectionCascade ButterworthCascade::design (Kind kind, double cutoffHz, int order, double sampleRate) noexcept
{
    // Not implemented yet.
    (void) kind; (void) cutoffHz; (void) order; (void) sampleRate;
    return {};
}

double ButterworthCascade::sectionQ (int order, int k) noexcept
{
    // Not implemented yet.
    (void) order; (void) k;
    return 0.0;
}

double ButterworthCascade::analogMagnitudeDb (Kind kind, double frequencyHz, double cutoffHz, int order) noexcept
{
    const auto x = kind == Kind::highCut ? frequencyHz / cutoffHz : cutoffHz / frequencyHz;
    return -10.0 * std::log10 (1.0 + std::pow (x, 2.0 * order));
}
