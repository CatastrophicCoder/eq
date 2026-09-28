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

    Dynamic bands (M7, decisions 2026-09-28): a detector filter (band pass for a bell,
    lowpass/highpass for the shelves) feeds a LevelDetector per detection channel,
    from the band's own input or the side-chain. Every dynamicStep samples the gain
    law's change is added to the static gain and the coefficients are redesigned,
    per channel for Stereo bands.

    Real-time use: prepare() off the audio thread; setTargets(), process() and
    reset() do not allocate or lock.
*/
class EqBand
{
public:
    static constexpr int subBlockSize = 32;
    static constexpr double rampSeconds = 0.02;
    static constexpr double crossfadeSeconds = 0.02;
    static constexpr int dynamicStep = 16;
    static constexpr double detectorShelfQ = 0.71;

    EqBand();

    /** Jumps straight to the current targets, with no ramp or crossfade. */
    void prepare (double sampleRate, int numChannels);

    void setTargets (const BandSettings& newTargets) noexcept;

    /** Clears filter state and ends any crossfade, without changing parameters. */
    void reset() noexcept;

    /** Filters the buffer in place (at most CascadeProcessor::maxChannels channels).
        The side-chain (mono or stereo, same length) feeds the detector of a dynamic band
        set to side-chain; without one, such a band listens to its own input.
    */
    void process (juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain = nullptr) noexcept;

    /** Current dynamic gain change in dB per filter channel (0 for static bands); any thread. */
    float getLiveGainChangeDb (int channel) const noexcept
    {
        return liveGainChangeDb[static_cast<size_t> (juce::jlimit (0, 1, channel))].load (std::memory_order_relaxed);
    }

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

        /** Gain change the coefficients were last designed with; NaN after a static design. */
        std::array<double, 2> appliedChange {};
    };

    void startCrossfade() noexcept;
    void updateCoefficients() noexcept;
    BandSettings settingsFor (const Slot& slot) const noexcept;

    /** The slot whose settings drive detection: the active one if dynamic, else the incoming one. */
    const Slot* dynamicSlot() const noexcept;
    void updateDetector (const Slot& slot) noexcept;
    void applyDynamicGain() noexcept;
    void designDynamic (Slot& slot) noexcept;

    /** One input frame (or side-chain frame) into the detectors. */
    void detect (double l, double r, ChannelMode mode, bool twoChannels) noexcept;

    std::array<Slot, 2> slots;
    int active = 0;
    int fadeLength = 1, fadeRemaining = 0;

    double sampleRate = 44100.0;
    int numChannels = 2;
    BandSettings target;

    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> frequency, q;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> gainDb;

    // Dynamics (M7).
    bool detecting = false;
    CascadeProcessor detectorFilter;
    FilterType detectorType = FilterType::bell;
    double detectorFrequency = 0.0, detectorQ = 0.0;
    std::array<LevelDetector, 2> detectors;
    double appliedAttackMs = -1.0, appliedReleaseMs = -1.0;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> thresholdDb, rangeDb;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> ratio;
    std::array<double, 2> gainChange {};

    std::array<std::atomic<float>, 2> liveGainChangeDb {};
};
