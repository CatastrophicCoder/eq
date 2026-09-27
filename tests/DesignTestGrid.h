#pragma once

#include "dsp/BiquadCoefficients.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <functional>
#include <vector>

//==============================================================================
/** Shared test grid and vs-analog checker for the filter designs.

    Grids (decision 2026-09-28):
      - strict:   the CLAUDE.md grid, f0 = 100 Hz, 1 kHz, 5 kHz at 44.1/48/96 kHz,
                  and 10 kHz, 20 kHz at 192 kHz.
      - extended: strict plus f0 = 10 kHz and 16 kHz at 44.1 kHz.

    Test points at or above 0.8 of Nyquist use a separate, stated near-Nyquist bound.
*/
namespace DesignTestGrid
{
    struct Case
    {
        double sampleRate;
        double centreHz;
        bool inStrictGrid;
    };

    inline std::vector<Case> extendedCases()
    {
        std::vector<Case> cases;

        for (auto fs : { 44100.0, 48000.0, 96000.0 })
            for (auto f0 : { 100.0, 1000.0, 5000.0 })
                cases.push_back ({ fs, f0, true });

        for (auto f0 : { 10000.0, 20000.0 })
            cases.push_back ({ 192000.0, f0, true });

        for (auto f0 : { 10000.0, 16000.0 })
            cases.push_back ({ 44100.0, f0, false });

        return cases;
    }

    inline bool isNearNyquist (double frequencyHz, double sampleRate)
    {
        return frequencyHz >= 0.8 * sampleRate / 2.0;
    }

    inline const std::vector<double> gainsDb { -18.0, -12.0, -6.0, 6.0, 12.0, 18.0 };
    inline const std::vector<double> qs { 0.5, 0.71, 1.0, 4.0 };

    /** Stated bounds in dB vs the analog prototype. */
    struct Bounds
    {
        double centre;              // at f0, extended grid
        double octave;              // at +-1 octave below 0.8 Nyquist, extended grid
        double octaveNearNyquist;   // at +-1 octave at or above 0.8 Nyquist
        double strictCentre;        // at f0, strict grid
        double strictOctave;        // at +-1 octave, strict grid
    };

    using Design = std::function<BiquadCoefficients (double centreHz, double param, double sampleRate)>;
    using Analog = std::function<double (double frequencyHz, double centreHz, double param)>;

    /** Checks a design against its analog prototype at f0 and +-1 octave over the extended grid. */
    inline void checkAgainstAnalog (const Design& design, const Analog& analog,
                                    const std::vector<double>& params, const Bounds& bounds,
                                    bool checkCentre = true)
    {
        for (const auto& c : extendedCases())
        {
            for (auto p : params)
            {
                const auto coeffs = design (c.centreHz, p, c.sampleRate);

                if (checkCentre)
                {
                    INFO ("centre: fs=" << c.sampleRate << " f0=" << c.centreHz << " param=" << p);
                    const auto error = std::abs (coeffs.magnitudeDb (c.centreHz, c.sampleRate)
                                                 - analog (c.centreHz, c.centreHz, p));
                    CHECK (error <= bounds.centre);

                    if (c.inStrictGrid)
                        CHECK (error <= bounds.strictCentre);
                }

                for (auto f : { c.centreHz / 2.0, c.centreHz * 2.0 })
                {
                    if (f >= c.sampleRate / 2.0)
                        continue;

                    INFO ("octave: fs=" << c.sampleRate << " f0=" << c.centreHz << " param=" << p << " f=" << f);
                    const auto error = std::abs (coeffs.magnitudeDb (f, c.sampleRate) - analog (f, c.centreHz, p));

                    CHECK (error <= (isNearNyquist (f, c.sampleRate) ? bounds.octaveNearNyquist : bounds.octave));

                    if (c.inStrictGrid)
                        CHECK (error <= bounds.strictOctave);
                }
            }
        }
    }

    /** Parameter corners used for the stability checks. */
    inline const std::vector<double> stabilitySampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };
    inline const std::vector<double> stabilityFrequencies { 20.0, 100.0, 1000.0, 10000.0, 20000.0 };
    inline const std::vector<double> stabilityGains { -30.0, -12.0, 0.0, 12.0, 30.0 };
    inline const std::vector<double> stabilityQs { 0.1, 0.71, 4.0, 18.0 };

    inline bool isFiniteAndStable (const BiquadCoefficients& c)
    {
        return std::isfinite (c.b0) && std::isfinite (c.b1) && std::isfinite (c.b2)
            && std::isfinite (c.a1) && std::isfinite (c.a2) && c.isStable();
    }
}
