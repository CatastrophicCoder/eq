#pragma once

#include "SectionCascade.h"

#include <array>

//==============================================================================
/** Runs a SectionCascade on audio, one channel at a time.

    Each section is a transposed direct form II biquad in double precision (the
    same structure as juce::dsp::IIR::Filter). State lives in fixed arrays, so
    nothing here allocates.
*/
class CascadeProcessor
{
public:
    static constexpr int maxChannels = 2;

    /** Loads new coefficients, keeping the filter state. Sections that were not
        in use before start from zero state.
    */
    void setCoefficients (const SectionCascade& newCascade) noexcept;

    void reset() noexcept;

    double processSample (int channel, double input) noexcept;

    const SectionCascade& getCascade() const noexcept { return cascade; }

private:
    struct SectionState { double s1 = 0.0, s2 = 0.0; };

    SectionCascade cascade;
    std::array<std::array<SectionState, SectionCascade::maxSections>, maxChannels> state {};
};
