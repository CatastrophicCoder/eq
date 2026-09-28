#include "StereoTransfer.h"

#include "AutoGain.h"
#include "BandDesign.h"

StereoTransfer::Matrix StereoTransfer::identity() noexcept
{
    return { { { Complex { 1.0 }, Complex {} }, { Complex {}, Complex { 1.0 } } } };
}

StereoTransfer::Matrix StereoTransfer::multiply (const Matrix& a, const Matrix& b) noexcept
{
    Matrix r {};
    for (size_t i = 0; i < 2; ++i)
        for (size_t j = 0; j < 2; ++j)
            r[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j];
    return r;
}

StereoTransfer::Matrix StereoTransfer::forBand (ChannelMode mode, Complex h) noexcept
{
    const Complex one { 1.0 }, zero {};

    switch (mode)
    {
        case ChannelMode::stereo: return { { { h, zero }, { zero, h } } };
        case ChannelMode::left:   return { { { h, zero }, { zero, one } } };
        case ChannelMode::right:  return { { { one, zero }, { zero, h } } };

        // T^-1 diag (a, b) T with T = [[1/2, 1/2], [1/2, -1/2]]:
        //   [[(a + b)/2, (a - b)/2], [(a - b)/2, (a + b)/2]]
        case ChannelMode::mid:
        {
            const auto p = (h + one) / 2.0, m = (h - one) / 2.0;
            return { { { p, m }, { m, p } } };
        }
        case ChannelMode::side:
        {
            const auto p = (one + h) / 2.0, m = (one - h) / 2.0;
            return { { { p, m }, { m, p } } };
        }
    }

    return identity();
}

StereoTransfer::Matrix StereoTransfer::chain (std::span<const BandSettings> bands, double frequencyHz, double sampleRate,
                                              bool includeCuts)
{
    auto m = identity();

    for (const auto& b : bands)
    {
        if (! b.isActive() || (! includeCuts && ! AutoGain::countsTowardsAutoGain (b.type)))
            continue;

        m = multiply (forBand (b.channel, BandDesign::design (b, sampleRate).response (frequencyHz, sampleRate)), m);
    }

    return m;
}

double StereoTransfer::powerGain (const Matrix& m) noexcept
{
    double sum = 0.0;
    for (const auto& row : m)
        for (const auto& v : row)
            sum += std::norm (v);
    return 0.5 * sum;
}

StereoTransfer::Matrix StereoTransfer::toMidSide (const Matrix& m) noexcept
{
    const Complex half { 0.5 }, one { 1.0 };
    const Matrix t { { { half, half }, { half, -half } } };
    const Matrix tInverse { { { one, one }, { one, -one } } };
    return multiply (multiply (t, m), tInverse);
}
