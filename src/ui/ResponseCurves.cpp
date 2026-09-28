#include "ResponseCurves.h"

#include "dsp/BandDesign.h"
#include "dsp/StereoTransfer.h"

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
    std::array<SectionCascade, numBands> designs;

    for (size_t b = 0; b < static_cast<size_t> (numBands); ++b)
    {
        shown[b] = b < count && bands[b].inUse;
        active[b] = b < count && bands[b].isActive();
        lastBands[b] = b < count ? bands[b] : BandSettings {};

        if (! shown[b])
        {
            curves[b].fill (0.0);
            continue;
        }

        // A disabled band is drawn with the curve it would have when enabled.
        auto asIfEnabled = bands[b];
        asIfEnabled.enabled = true;
        const auto design = BandDesign::design (asIfEnabled, sampleRate);
        designs[b] = design;

        for (size_t k = 0; k < static_cast<size_t> (numPoints); ++k)
        {
            // Points at or above Nyquist (only at sample rates below 40 kHz) repeat the last valid value.
            const auto f = std::min (frequencies[k], 0.499 * sampleRate);
            curves[b][k] = design.magnitudeDb (f, sampleRate);
            if (active[b])
                sum[k] += curves[b][k];
        }
    }

    // Which sums to show (decision 2026-09-28): one for an all-Stereo chain; L and R when any
    // Left/Right band is active; M and S when only Mid/Side bands are; mixed: the L and R diagonal.
    auto anyLeftRight = false, anyMidSide = false;
    for (size_t b = 0; b < static_cast<size_t> (numBands); ++b)
        if (active[b])
        {
            anyLeftRight = anyLeftRight || ChannelModes::isLeftRight (lastBands[b].channel);
            anyMidSide = anyMidSide || ChannelModes::isMidSide (lastBands[b].channel);
        }

    layout = anyLeftRight ? SumLayout::leftRight : anyMidSide ? SumLayout::midSide : SumLayout::single;

    if (layout == SumLayout::single)
    {
        secondSum = sum;
    }
    else
    {
        auto toDb = [] (std::complex<double> v) { return 20.0 * std::log10 (std::max (1.0e-15, std::abs (v))); };

        for (size_t k = 0; k < static_cast<size_t> (numPoints); ++k)
        {
            const auto f = std::min (frequencies[k], 0.499 * sampleRate);
            auto m = StereoTransfer::identity();

            for (size_t b = 0; b < static_cast<size_t> (numBands); ++b)
                if (active[b])
                    m = StereoTransfer::multiply (StereoTransfer::forBand (lastBands[b].channel, designs[b].response (f, sampleRate)), m);

            if (layout == SumLayout::midSide)
                m = StereoTransfer::toMidSide (m);

            sum[k] = toDb (m[0][0]);
            secondSum[k] = toDb (m[1][1]);
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
