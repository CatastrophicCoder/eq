#pragma once

//==============================================================================
/** Which part of the stereo signal a band acts on (M6). The order is the index of
    the band<n>_channel parameter and is stored in sessions: never reorder.
*/
enum class ChannelMode
{
    stereo,
    left,
    right,
    mid,
    side
};

namespace ChannelModes
{
    inline constexpr int count = 5;
    inline constexpr const char* names[count] { "Stereo", "Left", "Right", "Mid", "Side" };

    /** Node badge; empty for Stereo. */
    inline constexpr const char* letters[count] { "", "L", "R", "M", "S" };

    inline constexpr bool isMidSide (ChannelMode m) noexcept { return m == ChannelMode::mid || m == ChannelMode::side; }
    inline constexpr bool isLeftRight (ChannelMode m) noexcept { return m == ChannelMode::left || m == ChannelMode::right; }
}
