#pragma once

#include "ChannelMode.h"
#include "DynamicGainLaw.h"
#include "LevelDetector.h"
#include "FilterType.h"

#include <juce_core/juce_core.h>

//==============================================================================
/** Everything that defines one band. Frequency, gain and Q are continuous
    (smoothed); type, slope, enabled and inUse are discrete (crossfaded).

    A band processes audio only when it is in use and enabled. "Not in use" means
    the slot is free (deleted); "in use but disabled" means bypassed but kept.
*/
struct BandSettings
{
    FilterType type = FilterType::bell;
    double frequencyHz = 1000.0;
    double gainDb = 0.0;
    double q = 0.71;
    int slopeIndex = 1;   // 12 dB/oct
    bool enabled = true;
    bool inUse = true;
    ChannelMode channel = ChannelMode::stereo;

    /** Dynamics (M7). Only Bell, Low Shelf and High Shelf can be dynamic. */
    struct Dynamics
    {
        bool on = false;
        DynamicGainLaw::Mode mode = DynamicGainLaw::Mode::range;
        double thresholdDb = -20.0;
        double rangeDb = -6.0;
        double ratio = 2.0;
        double attackMs = 10.0;
        double releaseMs = 100.0;
        LevelDetector::Mode detector = LevelDetector::Mode::peak;
        bool sidechain = false;
        bool spectral = false;   // 9g: act per frequency slice (SpectralDynamicsEngine) instead of on the whole band
    };
    Dynamics dynamics;

    bool isActive() const noexcept { return enabled && inUse; }

    static constexpr bool typeCanBeDynamic (FilterType t) noexcept
    {
        return t == FilterType::bell || t == FilterType::lowShelf || t == FilterType::highShelf;
    }

    /** Active, dynamics switched on, and a type that supports it. */
    bool isDynamic() const noexcept { return isActive() && dynamics.on && typeCanBeDynamic (type); }

    /** Dynamic with Spectral on (9g): the moving part runs per frequency slice. */
    bool isSpectral() const noexcept { return isDynamic() && dynamics.spectral; }

    /** Exact comparison, used to detect any parameter change. */
    bool isIdenticalTo (const BandSettings& other) const noexcept
    {
        return hasSameDiscreteSettings (other)
            && juce::exactlyEqual (frequencyHz, other.frequencyHz)
            && juce::exactlyEqual (gainDb, other.gainDb)
            && juce::exactlyEqual (q, other.q)
            && dynamics.mode == other.dynamics.mode && dynamics.detector == other.dynamics.detector
            && dynamics.sidechain == other.dynamics.sidechain && dynamics.spectral == other.dynamics.spectral
            && juce::exactlyEqual (dynamics.thresholdDb, other.dynamics.thresholdDb)
            && juce::exactlyEqual (dynamics.rangeDb, other.dynamics.rangeDb)
            && juce::exactlyEqual (dynamics.ratio, other.dynamics.ratio)
            && juce::exactlyEqual (dynamics.attackMs, other.dynamics.attackMs)
            && juce::exactlyEqual (dynamics.releaseMs, other.dynamics.releaseMs);
    }

    bool hasSameDiscreteSettings (const BandSettings& other) const noexcept
    {
        return type == other.type && slopeIndex == other.slopeIndex && enabled == other.enabled && inUse == other.inUse
            && channel == other.channel && dynamics.on == other.dynamics.on;
    }
};
