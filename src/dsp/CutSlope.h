#pragma once

//==============================================================================
/** Cut slopes: index 0-15 are 6-96 dB/oct (Butterworth orders 1-16), index 16 is
    Brickwall (order 32). The index is the band<n>_slope parameter value and is
    stored in saved sessions: never reorder.
*/
namespace CutSlope
{
    inline constexpr int count = 17;
    inline constexpr int brickwallIndex = 16;
    inline constexpr int brickwallOrder = 32;

    inline constexpr int order (int index) noexcept
    {
        return index >= brickwallIndex ? brickwallOrder : (index < 0 ? 1 : index + 1);
    }

    inline constexpr int dbPerOctave (int index) noexcept
    {
        return 6 * order (index);
    }

    inline constexpr const char* labels[count] { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct",
                                                 "36 dB/oct", "42 dB/oct", "48 dB/oct", "54 dB/oct", "60 dB/oct",
                                                 "66 dB/oct", "72 dB/oct", "78 dB/oct", "84 dB/oct", "90 dB/oct",
                                                 "96 dB/oct", "Brickwall" };
}
