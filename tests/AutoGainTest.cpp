#include "dsp/AutoGain.h"
#include "dsp/BandDesign.h"
#include "dsp/KWeighting.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>

using Catch::Matchers::WithinAbs;

namespace
{
    BandSettings band (FilterType type, double f, double gain, double q = 1.0, int slope = 3, bool enabled = true)
    {
        BandSettings s;
        s.type = type; s.frequencyHz = f; s.gainDb = gain; s.q = q; s.slopeIndex = slope; s.enabled = enabled;
        return s;
    }

    std::array<BandSettings, 16> allDisabled()
    {
        std::array<BandSettings, 16> bands;
        for (auto& b : bands)
            b.enabled = false;
        return bands;
    }

    /** Independent reference: the same K-weighted pink-noise power ratio, integrated with the
        trapezoidal rule over ln f on a much finer grid. */
    double referenceOffsetDb (const std::array<BandSettings, 16>& bands, double fs)
    {
        constexpr int n = 8192;
        double num = 0.0, den = 0.0;

        for (int k = 0; k < n; ++k)
        {
            const auto f = 20.0 * std::pow (1000.0, k / (n - 1.0));
            const auto w = std::pow (10.0, KWeighting::magnitudeDb (f) / 10.0) * ((k == 0 || k == n - 1) ? 0.5 : 1.0);

            double hDb = 0.0;
            for (const auto& b : bands)
                if (b.enabled && AutoGain::countsTowardsAutoGain (b.type))
                    hDb += BandDesign::design (b, fs).magnitudeDb (f, fs);

            num += w * std::pow (10.0, hDb / 10.0);
            den += w;
        }

        return std::clamp (-10.0 * std::log10 (num / den), -AutoGain::limitDb, AutoGain::limitDb);
    }
}

TEST_CASE ("K-weighting has the gain the standard states at 997 Hz", "[autogain]")
{
    // BS.1770-5, note 1 to eq. 2: the -0.691 constant cancels the K-weighting gain at 997 Hz.
    CHECK_THAT (KWeighting::magnitudeDb (997.0), WithinAbs (0.691, 0.005));
}

TEST_CASE ("K-weighting has its documented shape", "[autogain]")
{
    // Stage 1 is a ~+4 dB high shelf (BS.1770-5 fig. 2); stage 2 a high-pass that
    // attenuates the lowest octaves (fig. 4).
    CHECK (KWeighting::magnitudeDb (10000.0) > 3.5);
    CHECK (KWeighting::magnitudeDb (10000.0) < 4.5);
    CHECK (KWeighting::magnitudeDb (20.0) < -10.0);
    CHECK (KWeighting::magnitudeDb (200.0) > -0.5);
    CHECK (KWeighting::magnitudeDb (200.0) < 0.5);
}

TEST_CASE ("Auto Gain is zero when nothing counts", "[autogain]")
{
    CHECK_THAT (AutoGain::computeOffsetDb (allDisabled(), 48000.0), WithinAbs (0.0, 1e-9));

    auto bands = allDisabled();
    bands[0] = band (FilterType::bell, 1000.0, 0.0);
    CHECK_THAT (AutoGain::computeOffsetDb (bands, 48000.0), WithinAbs (0.0, 1e-6));
}

TEST_CASE ("Auto Gain ignores Low Cut and High Cut", "[autogain]")
{
    CHECK_FALSE (AutoGain::countsTowardsAutoGain (FilterType::lowCut));
    CHECK_FALSE (AutoGain::countsTowardsAutoGain (FilterType::highCut));
    for (auto t : { FilterType::bell, FilterType::lowShelf, FilterType::highShelf, FilterType::notch,
                    FilterType::bandPass, FilterType::tiltShelf, FilterType::flatTilt, FilterType::allPass })
        CHECK (AutoGain::countsTowardsAutoGain (t));

    auto bands = allDisabled();
    bands[0] = band (FilterType::lowCut, 200.0, 0.0, 0.71, 15);
    bands[1] = band (FilterType::highCut, 2000.0, 0.0, 0.71, 15);
    CHECK_THAT (AutoGain::computeOffsetDb (bands, 48000.0), WithinAbs (0.0, 1e-9));
}

TEST_CASE ("Auto Gain matches an independent fine-grid integration", "[autogain]")
{
    // 256-point sum vs 8192-point trapezoid: the discretisation must not matter at 0.05 dB.
    for (auto fs : { 44100.0, 48000.0, 96000.0 })
    {
        auto bands = allDisabled();
        bands[0] = band (FilterType::bell, 1000.0, 6.0, 1.0);
        bands[1] = band (FilterType::lowShelf, 120.0, -4.0, 0.71);
        bands[2] = band (FilterType::highShelf, 8000.0, 3.0, 0.71);
        bands[3] = band (FilterType::notch, 3000.0, 0.0, 8.0);
        bands[4] = band (FilterType::highCut, 12000.0, 0.0, 0.71, 3);   // excluded
        bands[5] = band (FilterType::flatTilt, 1000.0, -6.0);

        INFO ("fs=" << fs);
        CHECK_THAT (AutoGain::computeOffsetDb (bands, fs), WithinAbs (referenceOffsetDb (bands, fs), 0.05));
    }
}

TEST_CASE ("Auto Gain counters boosts and cuts in the right direction", "[autogain]")
{
    auto boost = allDisabled();
    boost[0] = band (FilterType::bell, 2000.0, 6.0, 1.0);
    auto cut = allDisabled();
    cut[0] = band (FilterType::bell, 2000.0, -6.0, 1.0);

    CHECK (AutoGain::computeOffsetDb (boost, 48000.0) < -0.5);
    CHECK (AutoGain::computeOffsetDb (cut, 48000.0) > 0.5);
}

TEST_CASE ("Auto Gain is limited to +-24 dB", "[autogain]")
{
    // Two +30 dB low shelves at 20 kHz raise nearly the whole band by 60 dB.
    auto loud = allDisabled();
    loud[0] = band (FilterType::lowShelf, 20000.0, 30.0, 0.71);
    loud[1] = band (FilterType::lowShelf, 20000.0, 30.0, 0.71);
    CHECK_THAT (AutoGain::computeOffsetDb (loud, 48000.0), WithinAbs (-24.0, 1e-9));

    auto quiet = allDisabled();
    quiet[0] = band (FilterType::lowShelf, 20000.0, -30.0, 0.71);
    quiet[1] = band (FilterType::lowShelf, 20000.0, -30.0, 0.71);
    CHECK_THAT (AutoGain::computeOffsetDb (quiet, 48000.0), WithinAbs (24.0, 1e-9));
}
