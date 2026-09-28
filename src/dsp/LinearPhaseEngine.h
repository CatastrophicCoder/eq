#pragma once

#include "LinearPhaseDesigner.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

//==============================================================================
/** Runs the linear-phase filters of LinearPhaseDesigner on stereo audio (M8).

    Uniformly partitioned overlap-save FFT convolution (partitions of blockSize,
    FFT size 2 blockSize; e.g. Wefers, "Partitioned convolution algorithms for
    real-time auralization", 2015, ch. 5). The input spectra live in a frequency-
    domain delay line that does not depend on the filter, so a new filter gives the
    right output at once; old and new outputs are crossfaded over crossfadeBlocks.
    Latency: blockSize samples on top of the filter's own numTaps / 2.

    Paths: out L = L * leftFromLeft (+ R * leftFromRight), out R = R * rightFromRight
    (+ L * rightFromLeft); the cross paths only while the filter has cross terms.

    Hand-over: submit() (designer thread) turns the taps into partition spectra in a
    spare, preallocated filter set; the audio thread takes it at a block boundary.
    process() and reset() do not allocate or lock.
*/
class LinearPhaseEngine
{
public:
    static constexpr int blockSize = 512;
    static constexpr int fftSize = 2 * blockSize;
    static constexpr int bins = blockSize + 1;
    static constexpr int crossfadeBlocks = 2;
    static constexpr int maxTaps = LinearPhaseDesigner::tapCounts.back();
    static constexpr int maxPartitions = maxTaps / blockSize;

    /** Total delay of a filter with numTaps taps through the engine. */
    static constexpr int latencyFor (int numTaps) noexcept { return LinearPhaseDesigner::latencyFor (numTaps) + blockSize; }

    LinearPhaseEngine();

    /** Message thread, audio stopped: clears the history and takes a submitted filter at once. */
    void prepare();

    /** Designer thread: hands over a filter; false while the previous one has not been taken. */
    bool submit (const LinearPhaseDesigner::Result& result);

    /** Audio thread: filters both channels of the buffer in place. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    /** Audio thread: clears the history (the filter stays). */
    void reset() noexcept;

    /** Length of the filter in use (0 before the first one); audio thread. */
    int getCurrentTaps() const noexcept { return sets[static_cast<size_t> (current)].numTaps; }

    /** Filters taken so far; any thread. */
    int getSwapCount() const noexcept { return swaps.load (std::memory_order_relaxed); }

private:
    struct FilterSet
    {
        int numTaps = 0, partitions = 0;
        bool hasCross = false;
        std::array<std::vector<float>, 4> spectra;   // [path] interleaved complex, partition-major
    };

    enum Path { leftFromLeft, leftFromRight, rightFromLeft, rightFromRight };
    enum SlotState { empty, filling, ready };

    void takeSubmitted() noexcept;
    void processBlock() noexcept;
    void convolve (const FilterSet& set, int output, float* timeOut) noexcept;

    std::array<FilterSet, 3> sets;
    int current = 0, previous = 1, spare = 2;
    std::atomic<int> spareState { empty };
    std::atomic<int> swaps { 0 };

    juce::dsp::FFT fft { juce::roundToInt (std::log2 (fftSize)) };
    juce::dsp::FFT designerFft { juce::roundToInt (std::log2 (fftSize)) };
    std::vector<float> designerWork;

    // Frequency-domain delay line per input channel: maxPartitions spectra, newest at head.
    std::array<std::vector<float>, 2> history;
    int head = 0;

    std::array<std::array<float, blockSize>, 2> inputBlock {}, previousInput {}, outputBlock {};
    std::array<float, 2 * fftSize> work {};
    std::array<float, 2 * fftSize> accumulator {};
    std::array<float, blockSize> fadeScratch {};
    int position = 0;
    int fadeRemaining = 0;
};
