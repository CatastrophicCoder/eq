#include "AnalyzerFifo.h"

#include <algorithm>

void AnalyzerFifo::prepare (int numChannels, int capacity)
{
    // AbstractFifo keeps one slot free, so allocate one more than the usable capacity.
    buffer.setSize (numChannels, capacity + 1);
    buffer.clear();
    fifo.setTotalSize (capacity + 1);
    fifo.reset();
}

void AnalyzerFifo::reset() noexcept
{
    fifo.reset();
}

int AnalyzerFifo::push (const float* const* channels, int numChannels, int numSamples) noexcept
{
    const auto n = std::min (numSamples, fifo.getFreeSpace());
    if (n <= 0)
        return 0;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (n, start1, size1, start2, size2);

    const auto chans = std::min (numChannels, buffer.getNumChannels());
    for (int ch = 0; ch < chans; ++ch)
    {
        if (size1 > 0) buffer.copyFrom (ch, start1, channels[ch], size1);
        if (size2 > 0) buffer.copyFrom (ch, start2, channels[ch] + size1, size2);
    }

    fifo.finishedWrite (size1 + size2);
    return size1 + size2;
}

int AnalyzerFifo::pull (juce::AudioBuffer<float>& dest) noexcept
{
    const auto n = std::min (dest.getNumSamples(), fifo.getNumReady());
    if (n <= 0)
        return 0;

    int start1, size1, start2, size2;
    fifo.prepareToRead (n, start1, size1, start2, size2);

    const auto chans = std::min (dest.getNumChannels(), buffer.getNumChannels());
    for (int ch = 0; ch < chans; ++ch)
    {
        if (size1 > 0) dest.copyFrom (ch, 0, buffer, ch, start1, size1);
        if (size2 > 0) dest.copyFrom (ch, size1, buffer, ch, start2, size2);
    }

    fifo.finishedRead (size1 + size2);
    return size1 + size2;
}

int AnalyzerFifo::getNumReady() const noexcept
{
    return fifo.getNumReady();
}

int AnalyzerFifo::getCapacity() const noexcept
{
    return std::max (0, fifo.getTotalSize() - 1);
}
