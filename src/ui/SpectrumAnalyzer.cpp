#include "SpectrumAnalyzer.h"

#include "AnalyzerSettings.h"

#include <algorithm>
#include <cmath>
#include <numbers>

SpectrumAnalyzer::SpectrumAnalyzer()
{
    for (int k = 0; k < numPoints; ++k)
        frequencies[static_cast<size_t> (k)] = 20.0 * std::pow (1000.0, k / (numPoints - 1.0));

    levels.fill (floorDb);
    setFftOrder (12);
}

void SpectrumAnalyzer::setFftOrder (int order)
{
    if (fft != nullptr && order == fftOrder)
        return;

    fftOrder = order;
    fftSize = 1 << order;
    fft = std::make_unique<juce::dsp::FFT> (order);

    // Periodic Hann window; its coherent gain is 0.5.
    window.resize (static_cast<size_t> (fftSize));
    for (int n = 0; n < fftSize; ++n)
        window[static_cast<size_t> (n)] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * n / fftSize));

    history.assign (static_cast<size_t> (fftSize), 0.0f);
    work.assign (static_cast<size_t> (2 * fftSize), 0.0f);
    writePosition = 0;
}

void SpectrumAnalyzer::addSamples (const float* samples, int numSamples)
{
    // Keep only the newest fftSize samples, in a ring.
    const auto start = std::max (0, numSamples - fftSize);
    for (int i = start; i < numSamples; ++i)
    {
        history[static_cast<size_t> (writePosition)] = samples[i];
        writePosition = (writePosition + 1) % fftSize;
    }
}

void SpectrumAnalyzer::update (double sampleRate, double elapsedSeconds)
{
    if (frozen)
        return;

    // Newest fftSize samples, oldest first, windowed.
    for (int n = 0; n < fftSize; ++n)
        work[static_cast<size_t> (n)] = history[static_cast<size_t> ((writePosition + n) % fftSize)] * window[static_cast<size_t> (n)];
    std::fill (work.begin() + fftSize, work.end(), 0.0f);

    fft->performFrequencyOnlyForwardTransform (work.data());

    // A sine of amplitude A on a bin gives |X| = A N / 4 with a Hann window: scale by 4 / N.
    const auto scale = 4.0 / fftSize;
    const auto binWidth = sampleRate / fftSize;
    const auto lastBin = fftSize / 2;
    auto binDb = [&] (int b) { return 20.0 * std::log10 (std::max (1.0e-12, work[static_cast<size_t> (b)] * scale)); };

    const auto fall = release * elapsedSeconds;

    for (int k = 0; k < numPoints; ++k)
    {
        const auto f = frequencies[static_cast<size_t> (k)];
        const auto lo = k > 0 ? std::sqrt (f * frequencies[static_cast<size_t> (k - 1)]) : f;
        const auto hi = k < numPoints - 1 ? std::sqrt (f * frequencies[static_cast<size_t> (k + 1)]) : f;

        const auto firstBin = static_cast<int> (std::ceil (lo / binWidth));
        const auto lastInRange = std::min (lastBin, static_cast<int> (std::floor (hi / binWidth)));

        double target;
        if (firstBin <= lastInRange)
        {
            // Several bins in this point's range: the highest keeps peaks at their level.
            target = floorDb;
            for (int b = firstBin; b <= lastInRange; ++b)
                target = std::max (target, binDb (b));
        }
        else
        {
            // Bins sparser than points: interpolate (in dB) between the two nearest bins.
            const auto position = std::min (f / binWidth, static_cast<double> (lastBin));
            const auto b0 = static_cast<int> (std::floor (position));
            const auto b1 = std::min (b0 + 1, lastBin);
            const auto t = position - b0;
            target = (1.0 - t) * binDb (b0) + t * binDb (b1);
        }

        target = std::max (target, floorDb);

        // Instant rise, fall at the release rate.
        auto& level = levels[static_cast<size_t> (k)];
        level = std::max (target, std::max (floorDb, level - fall));
    }
}

void SpectrumAnalyzer::reset()
{
    std::fill (history.begin(), history.end(), 0.0f);
    writePosition = 0;
    levels.fill (floorDb);
}

double SpectrumAnalyzer::displayDb (int point) const noexcept
{
    const auto k = static_cast<size_t> (point);
    return levels[k] + AnalyzerSettings::tiltDbPerOctave * std::log2 (frequencies[k] / AnalyzerSettings::tiltPivotHz);
}
