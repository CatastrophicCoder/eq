#pragma once

#include "BandSettings.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

//==============================================================================
/** Spectral dynamics (M9g, decisions 2026-09-29): dynamic bands with Spectral on act
    per frequency slice instead of on the whole band.

    Short-time FFT with overlap-add: frames of fftSize samples every hop samples,
    square-root periodic Hann windows on analysis and synthesis (their product sums
    to a constant at 75 % overlap), so with no gain change the output is the input
    delayed by exactly latencySamples. Per frame and spectral band, each slice's
    level (the input's, or the side-chain's for side-chain bands; absolute, in dBFS
    for a sine) goes through a linear attack/release follower, the band's gain law
    gives a change, and the change is weighted by the band's region (regionWeight:
    the power response of the band pass for a bell, lowpass/highpass for a shelf).
    Stereo bands work per channel; Mid/Side bands on M or S. Bands apply in order.

    prepare() allocates; setBands(), process() and reset() do not.
*/
class SpectralDynamicsEngine
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hop = fftSize / 4;
    static constexpr int bins = fftSize / 2 + 1;
    static constexpr int latencySamples = fftSize;
    static constexpr int numBands = 16;

    void prepare (double sampleRate);
    void reset() noexcept;

    /** Audio thread: the current band settings (only spectral bands are used). */
    void setBands (const std::array<BandSettings, numBands>& bands) noexcept;

    /** Audio thread: processes two channels in place (a mono side-chain feeds both detection channels). */
    void process (juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain) noexcept;

    /** Latest gain change in dB of a band at a slice (the larger of the channels); any thread. */
    float getSliceGainDb (int band, int bin) const noexcept;

    /** 0..1: how much a band's dynamics act at a frequency (1 at a bell's centre). */
    static double regionWeight (const BandSettings& band, double frequencyHz) noexcept;

    static bool anySpectral (const std::array<BandSettings, numBands>& bands) noexcept;

private:
    void processFrame() noexcept;

    double sampleRate = 48000.0;
    std::array<BandSettings, numBands> bands {};
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;

    std::array<std::vector<float>, 2> input, sidechainInput, overlap, output;   // per channel
    std::array<std::vector<float>, 2> spectrum, sidechainSpectrum;               // interleaved complex
    std::vector<float> work;
    std::vector<double> envelope;       // [band][channel][bin] linear levels
    std::vector<double> gainDb;         // [bin] scratch per channel
    std::array<std::array<std::atomic<float>, bins>, numBands> published {};
    int position = 0;
    bool useSidechain = false;
};
