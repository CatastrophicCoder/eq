#include "KWeighting.h"

#include "BiquadCoefficients.h"

namespace
{
    constexpr double referenceRate = 48000.0;

    // ITU-R BS.1770-5, Table 1: stage 1 of the pre-filter (spherical head), 48 kHz.
    constexpr BiquadCoefficients stage1 { 1.53512485958697, -2.69169618940638, 1.19839281085285,
                                          -1.69065929318241, 0.73248077421585 };

    // ITU-R BS.1770-5, Table 2: second stage weighting curve (RLB high-pass), 48 kHz.
    constexpr BiquadCoefficients stage2 { 1.0, -2.0, 1.0,
                                          -1.99004745483398, 0.99007225036621 };
}

double KWeighting::magnitudeDb (double frequencyHz) noexcept
{
    return stage1.magnitudeDb (frequencyHz, referenceRate) + stage2.magnitudeDb (frequencyHz, referenceRate);
}
