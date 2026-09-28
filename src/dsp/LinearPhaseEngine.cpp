#include "LinearPhaseEngine.h"

#include <algorithm>

LinearPhaseEngine::LinearPhaseEngine()
{
    for (auto& set : sets)
        for (auto& s : set.spectra)
            s.assign (static_cast<size_t> (maxPartitions * bins * 2), 0.0f);

    for (auto& h : history)
        h.assign (static_cast<size_t> (maxPartitions * bins * 2), 0.0f);

    designerWork.assign (static_cast<size_t> (2 * fftSize), 0.0f);
}

void LinearPhaseEngine::prepare()
{
    reset();
    takeSubmitted();
    fadeRemaining = 0;   // a fresh stream starts with the new filter directly
}

bool LinearPhaseEngine::submit (const LinearPhaseDesigner::Result& result)
{
    auto expected = static_cast<int> (empty);
    if (! spareState.compare_exchange_strong (expected, filling))
        return false;

    auto& set = sets[static_cast<size_t> (spare)];
    set.numTaps = std::min (result.numTaps, maxTaps);
    set.partitions = set.numTaps / blockSize;
    set.hasCross = result.hasCrossTerms;

    const std::vector<float>* taps[] { &result.leftFromLeft, &result.leftFromRight, &result.rightFromLeft, &result.rightFromRight };

    for (int path = 0; path < 4; ++path)
    {
        auto& spectrum = set.spectra[static_cast<size_t> (path)];
        const auto* source = taps[path];
        const auto used = source->size() >= static_cast<size_t> (set.numTaps);

        for (int p = 0; p < set.partitions; ++p)
        {
            // Partition p: taps [p B, (p + 1) B), zero-padded to the FFT size.
            std::fill (designerWork.begin(), designerWork.end(), 0.0f);
            if (used)
                std::copy_n (source->data() + p * blockSize, blockSize, designerWork.data());
            designerFft.performRealOnlyForwardTransform (designerWork.data(), true);
            std::copy_n (designerWork.data(), bins * 2, spectrum.data() + p * bins * 2);
        }
    }

    spareState.store (ready, std::memory_order_release);
    return true;
}

void LinearPhaseEngine::takeSubmitted() noexcept
{
    // Only between crossfades: the set being faded out must stay intact until the fade ends.
    if (fadeRemaining > 0 || spareState.load (std::memory_order_acquire) != ready)
        return;

    const auto freed = previous;
    previous = current;
    current = spare;
    spare = freed;
    // Crossfade only between filters of the same length: a different length has a different delay,
    // and the processor mutes around length changes instead.
    const auto sameLength = sets[static_cast<size_t> (previous)].numTaps == sets[static_cast<size_t> (current)].numTaps;
    fadeRemaining = sameLength ? crossfadeBlocks * blockSize : 0;

    spareState.store (empty, std::memory_order_release);
    swaps.fetch_add (1, std::memory_order_relaxed);
}

void LinearPhaseEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto numSamples = buffer.getNumSamples();
    auto* channels = buffer.getArrayOfWritePointers();
    const auto numChannels = std::min (2, buffer.getNumChannels());

    for (int i = 0; i < numSamples;)
    {
        const auto chunk = std::min (blockSize - position, numSamples - i);

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto source = std::min (ch, numChannels - 1);
            auto* in = inputBlock[static_cast<size_t> (ch)].data() + position;
            std::copy_n (channels[source] + i, chunk, in);
        }

        for (int ch = 0; ch < numChannels; ++ch)
            std::copy_n (outputBlock[static_cast<size_t> (ch)].data() + position, chunk, channels[ch] + i);

        position += chunk;
        i += chunk;

        if (position == blockSize)
        {
            processBlock();
            position = 0;
        }
    }
}

void LinearPhaseEngine::processBlock() noexcept
{
    takeSubmitted();

    // New input spectra: FFT of [previous block, this block] into the delay line.
    head = (head + 1) % maxPartitions;
    for (size_t ch = 0; ch < 2; ++ch)
    {
        std::copy (previousInput[ch].begin(), previousInput[ch].end(), work.begin());
        std::copy (inputBlock[ch].begin(), inputBlock[ch].end(), work.begin() + blockSize);
        fft.performRealOnlyForwardTransform (work.data(), true);
        std::copy_n (work.data(), bins * 2, history[ch].data() + head * bins * 2);
        previousInput[ch] = inputBlock[ch];
    }

    const auto& now = sets[static_cast<size_t> (current)];

    for (int output = 0; output < 2; ++output)
    {
        auto& out = outputBlock[static_cast<size_t> (output)];

        if (now.numTaps == 0)
        {
            out.fill (0.0f);
            continue;
        }

        convolve (now, output, out.data());

        if (fadeRemaining > 0)
        {
            // Linear crossfade from the previous filter, both on the same input history.
            convolve (sets[static_cast<size_t> (previous)], output, fadeScratch.data());
            const auto length = static_cast<float> (crossfadeBlocks * blockSize);
            const auto done = static_cast<float> (crossfadeBlocks * blockSize - fadeRemaining);
            for (int n = 0; n < blockSize; ++n)
            {
                const auto g = (done + static_cast<float> (n + 1)) / length;
                out[static_cast<size_t> (n)] = g * out[static_cast<size_t> (n)] + (1.0f - g) * fadeScratch[static_cast<size_t> (n)];
            }
        }
    }

    if (fadeRemaining > 0)
        fadeRemaining -= blockSize;
}

void LinearPhaseEngine::convolve (const FilterSet& set, int output, float* timeOut) noexcept
{
    // Y = sum over partitions p and inputs of X_in[head - p] * H_path[p]; then the last B samples of the IFFT.
    std::fill (accumulator.begin(), accumulator.end(), 0.0f);

    const Path direct = output == 0 ? leftFromLeft : rightFromRight;
    const Path crossPath = output == 0 ? leftFromRight : rightFromLeft;
    const int directInput = output, crossInput = 1 - output;

    auto accumulate = [&] (Path path, int input)
    {
        const auto* h = set.spectra[static_cast<size_t> (path)].data();
        const auto* xs = history[static_cast<size_t> (input)].data();

        for (int p = 0; p < set.partitions; ++p)
        {
            const auto slot = (head - p + maxPartitions) % maxPartitions;
            const auto* x = xs + slot * bins * 2;
            const auto* hp = h + p * bins * 2;

            for (int k = 0; k < bins * 2; k += 2)
            {
                const auto xr = x[k], xi = x[k + 1], hr = hp[k], hi = hp[k + 1];
                accumulator[static_cast<size_t> (k)] += xr * hr - xi * hi;
                accumulator[static_cast<size_t> (k + 1)] += xr * hi + xi * hr;
            }
        }
    };

    accumulate (direct, directInput);
    if (set.hasCross)
        accumulate (crossPath, crossInput);

    fft.performRealOnlyInverseTransform (accumulator.data());
    std::copy_n (accumulator.data() + blockSize, blockSize, timeOut);
}

void LinearPhaseEngine::reset() noexcept
{
    for (auto& h : history)
        std::fill (h.begin(), h.end(), 0.0f);
    for (auto* blocks : { &inputBlock, &previousInput, &outputBlock })
        for (auto& b : *blocks)
            b.fill (0.0f);
    position = 0;
    head = 0;
}
