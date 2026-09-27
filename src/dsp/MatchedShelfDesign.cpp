#include "MatchedShelfDesign.h"

#include <cmath>
#include <numbers>

BiquadCoefficients MatchedShelfDesign::designHigh (double cornerHz, double gainDb, double sampleRate) noexcept
{
    // Unity gain is a singular case; the paper substitutes G = 1.00001 (appendix A.1).
    auto G = std::pow (10.0, gainDb / 20.0);
    if (std::abs (1.0 - G) < 1.0e-6)
        G = 1.00001;

    // Frequencies in units of Nyquist (section 2).
    const auto fc = cornerHz / (sampleRate / 2.0);
    const auto fc4 = fc * fc * fc * fc;

    const auto analogSquared = [&] (double f) { const auto f4 = f * f * f * f; return (fc4 + f4 * G) / (fc4 + f4 / G); };

    // Match at Nyquist, eq. 10, and at f1, f2 from eq. 11.
    const auto hNy = analogSquared (1.0);
    const auto f1 = fc / std::sqrt (0.160 + 1.543 * fc * fc);
    const auto f2 = fc / std::sqrt (0.947 + 3.806 * fc * fc);
    const auto h1 = analogSquared (f1);
    const auto h2 = analogSquared (f2);
    const auto phi1 = std::pow (std::sin (std::numbers::pi / 2.0 * f1), 2.0);
    const auto phi2 = std::pow (std::sin (std::numbers::pi / 2.0 * f2), 2.0);

    // Linear system for alpha1, alpha2: eqs. 12-15.
    const auto d1 = (h1 - 1.0) * (1.0 - phi1);
    const auto d2 = (h2 - 1.0) * (1.0 - phi2);
    const auto c11 = -phi1 * d1;
    const auto c21 = -phi2 * d2;
    const auto c12 = (hNy - h1) * phi1 * phi1;
    const auto c22 = (hNy - h2) * phi2 * phi2;

    const auto alpha1 = (c22 * d1 - c12 * d2) / (c11 * c22 - c12 * c21);
    const auto alpha2 = (d1 - c11 * alpha1) / c12;

    // Maximal flatness at DC (eq. 8) and the Nyquist match (eq. 9).
    const auto beta1 = alpha1;
    const auto beta2 = hNy * alpha2;

    // Back to A_i, B_i (eq. 7, with A0 = B0 = 1) and to the coefficients (eq. 5).
    const auto A1 = alpha2;
    const auto A2 = 0.25 * (alpha1 - alpha2);
    const auto B1 = beta2;
    const auto B2 = 0.25 * (beta1 - beta2);

    const auto V = 0.5 * (1.0 + std::sqrt (A1));
    const auto W = 0.5 * (1.0 + std::sqrt (B1));
    const auto a0 = 0.5 * (V + std::sqrt (V * V + A2));
    const auto b0 = 0.5 * (W + std::sqrt (W * W + B2));

    BiquadCoefficients c;
    c.a1 = (1.0 - V) / a0;
    c.a2 = -0.25 * A2 / a0 / a0;
    c.b0 = b0 / a0;
    c.b1 = (1.0 - W) / a0;
    c.b2 = -0.25 * B2 / b0 / a0;
    return c;
}

BiquadCoefficients MatchedShelfDesign::designLow (double cornerHz, double gainDb, double sampleRate) noexcept
{
    // Section 4: a high shelf with gain 1/G, numerator scaled by G.
    auto c = designHigh (cornerHz, -gainDb, sampleRate);
    const auto G = std::pow (10.0, gainDb / 20.0);

    c.b0 *= G;
    c.b1 *= G;
    c.b2 *= G;
    return c;
}


double MatchedShelfDesign::analogHighMagnitudeDb (double frequencyHz, double cornerHz, double gainDb) noexcept
{
    // Eq. 1 with g = G^(1/4), s = j f/fc  ->  |H|^2 = (1 + G x^4) / (1 + x^4 / G), x = f/fc
    const auto G = std::pow (10.0, gainDb / 20.0);
    const auto x4 = std::pow (frequencyHz / cornerHz, 4.0);
    return 10.0 * std::log10 ((1.0 + G * x4) / (1.0 + x4 / G));
}

double MatchedShelfDesign::analogLowMagnitudeDb (double frequencyHz, double cornerHz, double gainDb) noexcept
{
    // A low shelf with gain G is G times a high shelf with gain 1/G.
    return gainDb + analogHighMagnitudeDb (frequencyHz, cornerHz, -gainDb);
}
