#pragma once

//==============================================================================
/** Band filter types. The order is the index of the band<n>_type parameter and is
    stored in saved sessions: append new types at the end, never reorder.
*/
enum class FilterType
{
    bell,
    lowShelf,
    highShelf,
    lowCut,
    highCut,
    notch,
    bandPass,
    tiltShelf,
    flatTilt,
    allPass
};

namespace FilterTypes
{
    inline constexpr int count = 10;

    inline constexpr const char* names[count] { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut",
                                                "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt", "All Pass" };

    inline constexpr bool usesGain (FilterType t) noexcept
    {
        return t == FilterType::bell || t == FilterType::lowShelf || t == FilterType::highShelf
            || t == FilterType::tiltShelf || t == FilterType::flatTilt;
    }

    inline constexpr bool usesQ (FilterType t) noexcept
    {
        return t == FilterType::bell || t == FilterType::notch || t == FilterType::bandPass || t == FilterType::allPass;
    }

    inline constexpr bool usesSlope (FilterType t) noexcept
    {
        return t == FilterType::lowCut || t == FilterType::highCut;
    }
}
