#pragma once

#include "dsp/BandSettings.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

//==============================================================================
/** Background thread that keeps the Auto Gain offset current.

    Every pollIntervalMs it snapshots the band parameters; when they (or the
    sample rate) changed, it recomputes AutoGain::computeOffsetDb and publishes
    the result through an atomic. The audio thread only reads that atomic.
*/
class AutoGainUpdater final : private juce::Thread
{
public:
    static constexpr int pollIntervalMs = 20;

    AutoGainUpdater (juce::AudioProcessorValueTreeState& state, const std::array<std::atomic<bool>, 16>& bandInUse);
    ~AutoGainUpdater() override;

    void setSampleRate (double newSampleRate) noexcept;

    /** Latest computed offset in dB (independent of whether Auto Gain is switched on). */
    float getOffsetDb() const noexcept { return offsetDb.load (std::memory_order_relaxed); }

private:
    void run() override;
    std::array<BandSettings, 16> snapshot() const;

    struct BandParameters
    {
        std::atomic<float>* frequency;
        std::atomic<float>* gain;
        std::atomic<float>* q;
        std::atomic<float>* type;
        std::atomic<float>* slope;
        std::atomic<float>* enabled;
        std::atomic<float>* channel;
    };

    std::array<BandParameters, 16> bandParameters {};
    const std::array<std::atomic<bool>, 16>& inUse;
    std::atomic<double> sampleRate { 48000.0 };
    std::atomic<float> offsetDb { 0.0f };
};
