#pragma once

#include <cmath>
#include <numbers>

//==============================================================================
/** Building blocks shared by the matched designs.

    Source: Vicanek, "Matched Second Order Digital Filters" (2016).
*/
namespace MatchedDesignMath
{
    /** Normalised angular frequency, radians per sample. */
    inline double omega (double frequencyHz, double sampleRate) noexcept
    {
        return 2.0 * std::numbers::pi * frequencyHz / sampleRate;
    }

    struct Poles { double a1, a2; };

    /** Impulse-invariant poles of s^2 + 2 zeta w0 s + w0^2, eq. 12. */
    inline Poles impulseInvariantPoles (double w0, double zeta) noexcept
    {
        const auto decay = std::exp (-zeta * w0);
        const auto a1 = zeta <= 1.0 ? -2.0 * decay * std::cos  (std::sqrt (1.0 - zeta * zeta) * w0)
                                    : -2.0 * decay * std::cosh (std::sqrt (zeta * zeta - 1.0) * w0);
        return { a1, decay * decay };
    }

    /** Squared-magnitude denominator terms A0, A1, A2, eq. 27. */
    struct SquaredTerms { double t0, t1, t2; };

    inline SquaredTerms denominatorTerms (const Poles& p) noexcept
    {
        return { (1.0 + p.a1 + p.a2) * (1.0 + p.a1 + p.a2),
                 (1.0 - p.a1 + p.a2) * (1.0 - p.a1 + p.a2),
                 -4.0 * p.a2 };
    }

    /** phi0, phi1, phi2 at w, eq. 26. */
    struct Phi { double phi0, phi1, phi2; };

    inline Phi phi (double w) noexcept
    {
        const auto s = std::sin (w / 2.0);
        const auto phi1 = s * s;
        const auto phi0 = 1.0 - phi1;
        return { phi0, phi1, 4.0 * phi0 * phi1 };
    }

    /** A0 phi0 + A1 phi1 + A2 phi2: the squared denominator magnitude at w, eq. 25. */
    inline double evaluate (const SquaredTerms& t, const Phi& p) noexcept
    {
        return t.t0 * p.phi0 + t.t1 * p.phi1 + t.t2 * p.phi2;
    }
}
