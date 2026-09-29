#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>

#include <vector>

//==============================================================================
/** Long-term average spectrum for EQ Match (M9e): Hann-windowed FFT frames of
    2^fftOrder samples with 50 % overlap, power averaged per bin over everything
    added since reset(). levelsDb() reads it at any frequencies: the power interpolated
    between bins, averaged with the bins within +-1/24 octave (a steady tone therefore
    reads below its peak level; noise reads its density), scaled so a full-scale sine
    would read 0 dB in a single bin. UI thread only.
*/
class SpectrumAverager
{
public:
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr double bandOctaves = 1.0 / 24.0;   // levelsDb averages the bins within this distance

    void prepare (double sampleRate);
    void reset();

    void addSamples (const float* samples, int numSamples);

    double getSeconds() const noexcept;
    bool hasData() const noexcept { return frames > 0 || ! restoredHz.empty(); }

    std::vector<double> levelsDb (const std::vector<double>& frequenciesHz) const;

    /** Saved form (EQ Match, state version 6): the levels at statePoints log-spaced frequencies from
        20 Hz to 20 kHz (float32, base64) with the learned seconds. A restored averager reads those
        points (interpolated in log frequency) until the next reset. */
    static constexpr int statePoints = 1024;
    juce::ValueTree toState (const juce::Identifier& type) const;
    void fromState (const juce::ValueTree& state);

private:
    void analyseFrame();

    double sampleRate = 48000.0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, pending, work;
    std::vector<double> powerSum;
    juce::int64 samplesAdded = 0;
    int frames = 0;
    std::vector<double> restoredHz, restoredDb;
    double restoredSeconds = 0.0;
};
