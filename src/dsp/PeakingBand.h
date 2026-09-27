#pragma once

#include "BiquadCoefficients.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <vector>

//==============================================================================
/** One peaking (bell) band: matched coefficients, parameter smoothing and a
    per-channel filter.

    Real-time use: call prepare() off the audio thread. setTargets(), process()
    and reset() do not allocate or lock.

    While any parameter is ramping, coefficients are recomputed once per
    sub-block of subBlockSize samples; once all ramps finish they are left alone.
*/
class PeakingBand
{
public:
    static constexpr int subBlockSize = 32;
    static constexpr double rampSeconds = 0.02;

    PeakingBand();

    /** Allocates per-channel state and jumps straight to the current targets. */
    void prepare (double sampleRate, int numChannels);

    /** Sets new targets; the band ramps to them over rampSeconds. */
    void setTargets (double frequencyHz, double gainDb, double q) noexcept;

    /** Clears the filter state without changing parameters. */
    void reset() noexcept;

    /** Filters the buffer in place. Uses at most the prepared number of channels. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    /** The coefficients currently loaded into the filters. */
    const BiquadCoefficients& getCurrentCoefficients() const noexcept { return current; }

private:
    void updateCoefficients (double frequencyHz, double gainDb, double q) noexcept;

    using Filter = juce::dsp::IIR::Filter<double>;

    juce::dsp::IIR::Coefficients<double>::Ptr sharedCoefficients;
    std::vector<Filter> filters;
    BiquadCoefficients current;

    double sampleRate = 44100.0;
    double targetFrequency = 1000.0, targetGainDb = 0.0, targetQ = 0.71;

    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> frequency, q;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> gainDb;
};
