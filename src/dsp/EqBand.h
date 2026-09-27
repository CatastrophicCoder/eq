#pragma once

#include "BandSettings.h"
#include "CascadeProcessor.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

//==============================================================================
/** One EQ band of any type.

    Continuous parameters (frequency, gain, Q) ramp over rampSeconds; coefficients
    are recomputed once per sub-block while they ramp. Discrete parameters (type,
    slope, enabled) switch by crossfading linearly from the current filter to a
    freshly reset one over crossfadeSeconds. A discrete change that arrives during
    a crossfade waits for it to finish; the latest request wins.

    Real-time use: prepare() off the audio thread; setTargets(), process() and
    reset() do not allocate or lock.
*/
class EqBand
{
public:
    static constexpr int subBlockSize = 32;
    static constexpr double rampSeconds = 0.02;
    static constexpr double crossfadeSeconds = 0.02;

    EqBand();

    /** Jumps straight to the current targets, with no ramp or crossfade. */
    void prepare (double sampleRate, int numChannels);

    void setTargets (const BandSettings& newTargets) noexcept;

    /** Clears filter state and ends any crossfade, without changing parameters. */
    void reset() noexcept;

    /** Filters the buffer in place (at most CascadeProcessor::maxChannels channels). */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    bool isCrossfading() const noexcept { return fadeRemaining > 0; }

    /** The sections of the filter currently heard (the incoming one once a fade ends). */
    const SectionCascade& getActiveCascade() const noexcept;

    /** Discrete settings of the active filter plus the current (smoothed) continuous values. */
    BandSettings getCurrentSettings() const noexcept;

private:
    struct Slot
    {
        CascadeProcessor processor;
        BandSettings discrete;
    };

    void startCrossfade() noexcept;
    void updateCoefficients() noexcept;
    BandSettings settingsFor (const Slot& slot) const noexcept;

    std::array<Slot, 2> slots;
    int active = 0;
    int fadeLength = 1, fadeRemaining = 0;

    double sampleRate = 44100.0;
    int numChannels = 2;
    BandSettings target;

    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> frequency, q;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> gainDb;
};
