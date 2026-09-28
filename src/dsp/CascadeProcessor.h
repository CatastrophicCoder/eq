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

    /** Loads new coefficients for every channel, keeping the filter state. Sections
        that were not in use before start from zero state.
    */
    void setCoefficients (const SectionCascade& newCascade) noexcept;

    /** Loads coefficients for one channel only (dynamic bands, M7), same state rule. */
    void setChannelCoefficients (int channel, const SectionCascade& newCascade) noexcept;

    void reset() noexcept;

    double processSample (int channel, double input) noexcept;

    /** Channel 0's sections (all channels share them unless set per channel). */
    const SectionCascade& getCascade() const noexcept { return cascades[0]; }

private:
    struct SectionState { double s1 = 0.0, s2 = 0.0; };

    std::array<SectionCascade, maxChannels> cascades;
    std::array<std::array<SectionState, SectionCascade::maxSections>, maxChannels> state {};
};
