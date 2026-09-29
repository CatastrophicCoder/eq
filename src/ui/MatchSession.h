#pragma once

#include "dsp/SpectrumAverager.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

class ParametricEQAudioProcessor;

//==============================================================================
/** EQ Match state and actions (M9e, decisions 2026-09-29).

    Learning averages spectra between start and stop: with a side-chain connected,
    the side-chain is the reference and the plugin input the current sound, both in
    one pass; otherwise two capture passes of the plugin input. apply() fits the
    match (MatchCurve, via CurveFitter) into bands: replace all, or keep the
    existing bands and use the free slots (the match minus what they already do).
    One undo step. Message thread only.
*/
class MatchSession
{
public:
    enum class Learning { none, reference, current, both };
    enum class ApplyMode { replaceAll, keepExisting };
    static constexpr double minimumSeconds = 1.0;
    static constexpr const char* stateTag = "EQMatch";

    explicit MatchSession (ParametricEQAudioProcessor& processor);

    bool usesSidechain() const;

    /** Starting a pass ends any running one. "both" needs a side-chain. */
    void startLearning (Learning what);
    void stopLearning();
    Learning getLearning() const noexcept { return learning; }

    /** Samples from the plugin input tap and the side-chain tap (mono). */
    void addInputSamples (const float* samples, int numSamples);
    void addSidechainSamples (const float* samples, int numSamples);

    double getReferenceSeconds() const noexcept { return reference.getSeconds(); }
    double getCurrentSeconds() const noexcept { return current.getSeconds(); }
    bool canApply() const noexcept;

    void setAmount (double amount0to1) noexcept { amount = amount0to1; }
    void setSmoothingOctaves (double octaves) noexcept { smoothing = octaves; }
    double getAmount() const noexcept { return amount; }
    double getSmoothingOctaves() const noexcept { return smoothing; }

    /** The match curve at the given frequencies (zeros until both spectra are learned). */
    std::vector<double> curveDb (const std::vector<double>& frequenciesHz) const;

    int freeSlots() const;
    void apply (ApplyMode mode);

    /** Saved with the project (state version 6): both spectra, Amount and Smoothing. An invalid
        tree (older sessions) clears everything. */
    juce::ValueTree toState() const;
    void fromState (const juce::ValueTree& state);

private:
    ParametricEQAudioProcessor& processor;
    SpectrumAverager reference, current;
    Learning learning = Learning::none;
    double amount = 1.0, smoothing = 1.0 / 3.0;
};
