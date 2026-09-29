#include "SpectrumAverager.h"

#include <cmath>

void SpectrumAverager::prepare (double rate)
{
    sampleRate = rate;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.resize (static_cast<size_t> (fftSize));
    juce::dsp::WindowingFunction<float>::fillWindowingTables (window.data(), static_cast<size_t> (fftSize),
                                                              juce::dsp::WindowingFunction<float>::hann, false);
    work.assign (static_cast<size_t> (2 * fftSize), 0.0f);
    reset();
}

void SpectrumAverager::reset()
{
    pending.clear();
    powerSum.assign (static_cast<size_t> (fftSize / 2 + 1), 0.0);
    samplesAdded = 0;
    frames = 0;
}

void SpectrumAverager::addSamples (const float* samples, int numSamples)
{
    if (fft == nullptr || numSamples <= 0)
        return;

    pending.insert (pending.end(), samples, samples + numSamples);
    samplesAdded += numSamples;

    // Frames of fftSize with 50 % overlap.
    while (pending.size() >= static_cast<size_t> (fftSize))
    {
        analyseFrame();
        pending.erase (pending.begin(), pending.begin() + fftSize / 2);
    }
}

void SpectrumAverager::analyseFrame()
{
    std::fill (work.begin(), work.end(), 0.0f);
    for (int i = 0; i < fftSize; ++i)
        work[static_cast<size_t> (i)] = pending[static_cast<size_t> (i)] * window[static_cast<size_t> (i)];

    fft->performFrequencyOnlyForwardTransform (work.data(), true);

    for (size_t k = 0; k < powerSum.size(); ++k)
        powerSum[k] += static_cast<double> (work[k]) * static_cast<double> (work[k]);
    ++frames;
}

double SpectrumAverager::getSeconds() const noexcept
{
    return static_cast<double> (samplesAdded) / sampleRate;
}

std::vector<double> SpectrumAverager::levelsDb (const std::vector<double>& frequenciesHz) const
{
    // A sine of amplitude A peaks at |X| = A N / 4 with a Hann window (coherent gain 1/2): 0 dB for A = 1.
    const auto calibrationDb = 20.0 * std::log10 (4.0 / fftSize);
    std::vector<double> levels;
    levels.reserve (frequenciesHz.size());

    for (auto f : frequenciesHz)
    {
        if (frames == 0)
        {
            levels.push_back (-200.0);
            continue;
        }

        // Power at the frequency (interpolated between bins), averaged with the bins within
        // +-bandOctaves, which steadies the estimate where bins are dense.
        const auto last = static_cast<double> (powerSum.size() - 1);
        const auto bin = std::clamp (f * fftSize / sampleRate, 0.0, last);
        const auto lower = static_cast<size_t> (std::floor (bin));
        const auto upper = std::min (lower + 1, powerSum.size() - 1);
        const auto t = bin - static_cast<double> (lower);
        double sum = (1.0 - t) * powerSum[lower] + t * powerSum[upper];
        int count = 1;

        const auto from = static_cast<size_t> (std::ceil (std::clamp (bin * std::exp2 (-bandOctaves), 0.0, last)));
        const auto to = static_cast<size_t> (std::floor (std::clamp (bin * std::exp2 (bandOctaves), 0.0, last)));
        for (auto k = from; k <= to; ++k)
        {
            sum += powerSum[k];
            ++count;
        }

        const auto power = sum / count / frames;
        levels.push_back (power > 0.0 ? 10.0 * std::log10 (power) + calibrationDb : -200.0);
    }

    return levels;
}
