#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
/** Single-producer / single-consumer sample FIFO from the audio thread to the UI
    (juce::AbstractFifo over a preallocated buffer).

    prepare() allocates (not on the audio thread). push() is lock-free and
    allocation-free, and drops what does not fit rather than waiting.
*/
class AnalyzerFifo
{
public:
    void prepare (int numChannels, int capacity);
    void reset() noexcept;

    /** Audio thread. Returns the number of samples written (fewer if the FIFO is full). */
    int push (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** UI thread. Appends up to dest.getNumSamples() samples to dest, returns how many. */
    int pull (juce::AudioBuffer<float>& dest) noexcept;

    int getNumReady() const noexcept;
    int getCapacity() const noexcept;
    int getNumChannels() const noexcept { return buffer.getNumChannels(); }

private:
    juce::AbstractFifo fifo { 1 };
    juce::AudioBuffer<float> buffer;
};
