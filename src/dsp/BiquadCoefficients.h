#pragma once

//==============================================================================
/** Coefficients of one second-order section, normalised so that a0 == 1:

        H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)

    The default is the identity filter.
*/
struct BiquadCoefficients
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    /** Magnitude of H(e^jw) in dB at the given frequency. */
    double magnitudeDb (double frequencyHz, double sampleRate) const noexcept;

    /** True if both poles lie strictly inside the unit circle
        (|a1| < 1 + a2 and a2 < 1; Vicanek 2016, eq. 3).
    */
    bool isStable() const noexcept;
};
