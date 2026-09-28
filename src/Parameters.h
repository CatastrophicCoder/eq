#pragma once

#include "dsp/BandSettings.h"
#include "dsp/FilterType.h"

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/** Parameter IDs, defaults and the parameter layout.

    IDs carry the band index: band<n>_freq, _gain, _q, _type, _slope, _enabled, n = 1..16.
*/
namespace Parameters
{
    inline constexpr int numBands = 16;

    /** Output stage (M2 stage 4). */
    inline constexpr const char* outputGain   = "output_gain";
    inline constexpr const char* autoGain     = "auto_gain";
    inline constexpr const char* outputInvert = "output_invert";

    /** e.g. id (3, "freq") == "band3_freq". Fields: freq, gain, q, type, slope, enabled, channel. */
    juce::String id (int band, const char* field);

    /** Version hints for juce::ParameterID: band1_freq/gain/q date from M1 (1); everything
        added in M2 is 2. Never change a parameter's hint once released.
    */
    inline constexpr int m1VersionHint = 1;
    inline constexpr int m2VersionHint = 2;
    inline constexpr int m6VersionHint = 3;   // band<n>_channel
    inline constexpr int m7VersionHint = 4;   // dynamic parameters

    /** Dynamic fields (M7): dyn, dynmode, thresh, range, ratio, attack, release, detector, sidechain. */
    inline constexpr const char* dynamicFields[] { "dyn", "dynmode", "thresh", "range", "ratio",
                                                   "attack", "release", "detector", "sidechain" };

    struct BandDefaults
    {
        FilterType type;
        float frequencyHz;
        float gainDb;
        float q;
        int slopeIndex;
        bool enabled;
    };

    /** Defaults for a new instance (decision 2026-09-28): per-type presets, all disabled. */
    BandDefaults defaultsFor (int band);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** Turns one band's raw parameter values and its in-use flag into BandSettings
        (choice indices rounded and clamped). */
    BandSettings toBandSettings (float type, float frequencyHz, float gainDb, float q, float slope, float enabled,
                                 bool inUse, float channel) noexcept;
}
