#pragma once

//==============================================================================
/** How far a dynamic band's gain moves for a detected level (decision 2026-09-28).

    Range mode: the change grows from 0 at the threshold to the full range 12 dB
    above it, along a smoothstep (continuous slope at both ends).

    Ratio mode: compressor-style, (level - threshold)(1 - 1/ratio) with a 6 dB
    soft knee centred on the threshold, capped at |range|, in the direction of
    the range (negative range: cut when loud; positive: boost when loud).
*/
class DynamicGainLaw
{
public:
    enum class Mode { range, ratio };

    static constexpr double rangeSpanDb = 12.0;
    static constexpr double ratioKneeDb = 6.0;

    /** Gain change in dB to add to the band's static gain. */
    static double gainChangeDb (Mode mode, double levelDb, double thresholdDb, double rangeDb, double ratio) noexcept;
};
