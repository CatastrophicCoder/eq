#include "AutoGain.h"

#include "BandDesign.h"
#include "KWeighting.h"
#include "StereoTransfer.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    struct Grid
    {
        std::array<double, AutoGain::numPoints> frequency {};
        std::array<double, AutoGain::numPoints> weight {};   // |K(f)|^2
    };

    const Grid& grid()
    {
        static const Grid g = []
        {
            Grid result;
            for (int k = 0; k < AutoGain::numPoints; ++k)
            {
                const auto f = 20.0 * std::pow (1000.0, k / (AutoGain::numPoints - 1.0));
                result.frequency[static_cast<size_t> (k)] = f;
                result.weight[static_cast<size_t> (k)] = std::pow (10.0, KWeighting::magnitudeDb (f) / 10.0);
            }
            return result;
        }();

        return g;
    }
}

bool AutoGain::countsTowardsAutoGain (FilterType type) noexcept
{
    return type != FilterType::lowCut && type != FilterType::highCut;
}

double AutoGain::computeOffsetDb (std::span<const BandSettings> bands, double sampleRate) noexcept
{
    // Bands that count, with their designs, in chain order.
    std::array<SectionCascade, 16> designs;
    std::array<ChannelMode, 16> modes {};
    size_t numDesigns = 0;

    for (const auto& b : bands)
        if (b.isActive() && countsTowardsAutoGain (b.type) && numDesigns < designs.size())
        {
            modes[numDesigns] = b.channel;
            designs[numDesigns++] = BandDesign::design (b, sampleRate);
        }

    if (numDesigns == 0)
        return 0.0;

    const auto& g = grid();
    double weightedPower = 0.0, totalWeight = 0.0;

    for (size_t k = 0; k < g.frequency.size(); ++k)
    {
        const auto f = g.frequency[k];
        if (f >= 0.5 * sampleRate)
            continue;

        // 2x2 chain per frequency (decision 2026-09-28): power gain ||M||^2 / 2 for uncorrelated,
        // equal-level L and R. For an all-Stereo chain this is |H|^2, as before M6.
        auto m = StereoTransfer::identity();
        for (size_t i = 0; i < numDesigns; ++i)
            m = StereoTransfer::multiply (StereoTransfer::forBand (modes[i], designs[i].response (f, sampleRate)), m);

        weightedPower += g.weight[k] * StereoTransfer::powerGain (m);
        totalWeight += g.weight[k];
    }

    return std::clamp (-10.0 * std::log10 (weightedPower / totalWeight), -limitDb, limitDb);
}
