#pragma once

#include "BiquadCoefficients.h"

#include <array>

//==============================================================================
/** A fixed-capacity chain of second- (or first-) order sections, applied in series.
    Fixed size so a band can hold it without allocating.
*/
struct SectionCascade
{
    /** Enough for a 32nd-order Butterworth (Brickwall) or the 16-section Flat Tilt. */
    static constexpr int maxSections = 16;

    std::array<BiquadCoefficients, maxSections> sections {};
    int numSections = 0;

    void add (const BiquadCoefficients& section) noexcept;

    /** Product of the sections' responses at the given frequency. */
    std::complex<double> response (double frequencyHz, double sampleRate) const noexcept;

    /** Summed magnitude in dB of all sections at the given frequency. */
    double magnitudeDb (double frequencyHz, double sampleRate) const noexcept;

    bool isStable() const noexcept;
};
