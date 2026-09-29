#pragma once

#include <juce_dsp/juce_dsp.h>

#include <vector>

//==============================================================================
/** Long-term average spectrum for EQ Match (M9e): Hann-windowed FFT frames of
    2^fftOrder samples with 50 % overlap, power averaged per bin over everything
    added since reset(). levelsDb() reads it at any frequencies (interpolated
    between bins), calibrated so a full-scale sine reads 0 dB. UI thread only.
*/
class SpectrumAverager
{
public:
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;

    void prepare (double sampleRate);
    void reset();

    void addSamples (const float* samples, int numSamples);

    double getSeconds() const noexcept;
    bool hasData() const noexcept { return frames > 0; }

    std::vector<double> levelsDb (const std::vector<double>& frequenciesHz) const;

private:
    void analyseFrame();

    double sampleRate = 48000.0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, pending, work;
    std::vector<double> powerSum;
    juce::int64 samplesAdded = 0;
    int frames = 0;
};
