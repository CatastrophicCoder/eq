#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <vector>

//==============================================================================
/** UI-side spectrum analysis: keeps the newest samples, and on update() runs a
    Hann-windowed FFT (juce::dsp::FFT), converts to dB (a full-scale sine reads
    0 dB), groups bins into numPoints log-spaced points (20 Hz - 20 kHz; the
    highest bin in each point's range, interpolated where bins are sparser than
    points) and smooths over time: instant rise, fall at the release rate.
    Freeze holds the current values.
*/
class SpectrumAnalyzer
{
public:
    static constexpr int numPoints = 512;
    static constexpr double floorDb = -150.0;

    SpectrumAnalyzer();

    void setFftOrder (int order);
    int getFftSize() const noexcept { return fftSize; }
    void setReleaseDbPerSecond (double dbPerSecond) noexcept { release = dbPerSecond; }
    void setFrozen (bool shouldFreeze) noexcept { frozen = shouldFreeze; }
    bool isFrozen() const noexcept { return frozen; }

    /** Newest mono samples. */
    void addSamples (const float* samples, int numSamples);

    /** Analyses the newest fftSize samples and advances the smoothing by elapsedSeconds. */
    void update (double sampleRate, double elapsedSeconds);

    /** Clears samples and levels back to the floor. */
    void reset();

    double frequency (int point) const noexcept { return frequencies[static_cast<size_t> (point)]; }

    /** Smoothed level in dBFS at a point, without tilt. */
    double levelDb (int point) const noexcept { return levels[static_cast<size_t> (point)]; }

    /** Level with the display tilt (4.5 dB/oct around 1 kHz) applied. */
    double displayDb (int point) const noexcept;

private:
    int fftOrder = 12, fftSize = 4096;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, history, work;
    int writePosition = 0;
    double release = 25.0;
    bool frozen = false;

    std::array<double, numPoints> frequencies {};
    std::array<double, numPoints> levels {};
};
