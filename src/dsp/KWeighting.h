#pragma once

//==============================================================================
/** The K-weighting curve of ITU-R BS.1770 (loudness, LKFS/LUFS), used as a
    frequency-domain weight.

    Source: Recommendation ITU-R BS.1770-5 (11/2023), Table 1 (stage 1, head shelf)
    and Table 2 (stage 2, RLB high-pass). The standard defines both stages as
    biquads at 48 kHz; their magnitude at 20 Hz - 20 kHz is the weight, whatever
    the plugin's sample rate.
*/
class KWeighting
{
public:
    /** K-weighting magnitude in dB at the given frequency (valid below 24 kHz). */
    static double magnitudeDb (double frequencyHz) noexcept;
};
