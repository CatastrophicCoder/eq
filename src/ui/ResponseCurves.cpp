#include "ResponseCurves.h"

#include "dsp/BandDesign.h"

#include <algorithm>
#include <cmath>

ResponseCurves::ResponseCurves()
{
    for (int k = 0; k < numPoints; ++k)
        frequencies[static_cast<size_t> (k)] = 20.0 * std::pow (1000.0, k / (numPoints - 1.0));
}

bool ResponseCurves::update (std::span<const BandSettings> bands, double sampleRate)
{
    const auto count = std::min (bands.size(), static_cast<size_t> (numBands));

    auto unchanged = hasComputed && juce::exactlyEqual (sampleRate, lastSampleRate);
    for (size_t b = 0; b < count && unchanged; ++b)
        unchanged = bands[b].isIdenticalTo (lastBands[b]);

    if (unchanged)
        return false;

    sum.fill (0.0);

    for (size_t b = 0; b < static_cast<size_t> (numBands); ++b)
    {
        active[b] = b < count && bands[b].enabled;
        lastBands[b] = b < count ? bands[b] : BandSettings {};

        if (! active[b])
        {
            curves[b].fill (0.0);
            continue;
        }

        const auto design = BandDesign::design (bands[b], sampleRate);

        for (size_t k = 0; k < static_cast<size_t> (numPoints); ++k)
        {
            // Points at or above Nyquist (only at sample rates below 40 kHz) repeat the last valid value.
            const auto f = std::min (frequencies[k], 0.499 * sampleRate);
            curves[b][k] = design.magnitudeDb (f, sampleRate);
            sum[k] += curves[b][k];
        }
    }

    lastSampleRate = sampleRate;
    hasComputed = true;
    ++recomputes;
    return true;
}

double ResponseCurves::bandDb (int band, int point) const noexcept
{
    return curves[static_cast<size_t> (band)][static_cast<size_t> (point)];
}
