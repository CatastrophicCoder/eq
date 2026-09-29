#pragma once

#include "AutoGainUpdater.h"
#include "LinearPhaseUpdater.h"
#include "Parameters.h"
#include "presets/AbComparison.h"
#include "presets/PresetManager.h"
#include "presets/UndoHistory.h"
#include "ui/MatchSession.h"
#include "dsp/AnalyzerFifo.h"
#include "dsp/EqBand.h"
#include "dsp/LinearPhaseEngine.h"
#include "dsp/SpectralDynamicsEngine.h"
#include "ui/AnalyzerSettings.h"

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
class ParametricEQAudioProcessor final : public juce::AudioProcessor,
                                         private juce::Timer
{
public:
    //==============================================================================
    ParametricEQAudioProcessor();
    ~ParametricEQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    /** Clears filter state, crossfades and (M7) detector envelopes, e.g. after a transport jump. */
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Version written into every saved state; bump when the state format changes. */
    static constexpr int stateVersion = 6;   // 4: phase mode (M8); 5: A/B slots (M9a); 6: EQ Match spectra (M9e)

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }

    /** Latest Auto Gain offset from the background thread, in dB (whether or not Auto Gain is on). */
    float getAutoGainOffsetDb() const noexcept;

    /** Display range in dB (one of FrequencyAxis::ranges), stored in the session as the
        non-automatable state property "displayRangeDb". Message thread only.
    */
    double getDisplayRangeDb() const;
    void setDisplayRangeDb (double rangeDb);

    /** Whether a band's slot is in use (not deleted). Stored in the session as the hidden
        state property band<n>_used; an atomic copy serves the audio and Auto Gain threads.
        setBandInUse: message thread only.
    */
    bool isBandInUse (int band) const noexcept;
    void setBandInUse (int band, bool inUse);
    const std::array<std::atomic<bool>, 16>& getInUseFlags() const noexcept { return bandInUse; }

    /** Current settings of all 16 bands, including their in-use flags. */
    std::array<BandSettings, 16> getBandSettings() const;

    /** Phase mode (M8, decision 2026-09-29): hidden state properties "linearPhase" and
        "linearPhaseLength" (index into LinearPhaseDesigner::tapCounts). Linear phase reports
        LinearPhaseEngine::latencyFor (taps) samples of latency. Message thread only.
    */
    bool isLinearPhase() const;
    void setLinearPhase (bool shouldBeLinear);
    int getLinearPhaseLength() const;
    void setLinearPhaseLength (int index);

    /** Re-reads whether any band is spectral and reports the resulting latency (M9g). Message thread;
        a 10 Hz timer calls it too, since the Spectral switch is an automatable parameter. */
    void refreshLatency();

    /** True when the audio runs in the requested mode with its filter loaded and no fade pending; any thread. */
    bool isPhaseModeSettled() const noexcept;

    /** Number of linear-phase filters handed to the convolution so far (tests); any thread. */
    int getLinearPhaseSwapCount() const noexcept;

    /** A band's current dynamic gain change in dB per filter channel (M7); any thread. */
    float getLiveGainChangeDb (int band, int channel) const noexcept;

    //==============================================================================
    /** Analyzer taps (M5): stereo input (pre) and output (post), pushed from processBlock
        only while an editor is open. The editor is the single consumer.
    */
    void setAnalyzerActive (bool shouldBeActive) noexcept;
    bool isAnalyzerActive() const noexcept { return analyzerActive.load (std::memory_order_relaxed); }
    AnalyzerFifo& getPreFifo() noexcept  { return preFifo; }
    AnalyzerFifo& getPostFifo() noexcept { return postFifo; }

    /** EQ Match (M9e): the side-chain tap, fed only while active and a side-chain is connected. */
    AnalyzerFifo& getSidechainFifo() noexcept { return sidechainFifo; }
    void setSidechainTapActive (bool shouldBeActive) noexcept { sidechainTapActive.store (shouldBeActive, std::memory_order_relaxed); }
    bool isSidechainConnected() const;
    static constexpr int analyzerFifoCapacity = 32768;

    /** Presets (M6b). Message thread only. */
    PresetManager& getPresetManager() noexcept { return *presetManager; }

    /** A/B comparison (M9a). Message thread only. */
    AbComparison& getAbComparison() noexcept { return abComparison; }

    /** Undo/redo of sound edits (M9b). Message thread only. */
    UndoHistory& getUndoHistory() noexcept { return undoHistory; }

    /** EQ Match learned spectra and settings (M9e): kept with the plugin, saved with the project. Message thread only. */
    MatchSession& getMatchSession() noexcept { return matchSession; }

    /** The current preset's name and whether it is a factory preset, stored in the session. */
    juce::String getStoredPresetName() const;
    bool isStoredPresetFactory() const;
    void setStoredPreset (const juce::String& name, bool isFactory);

    /** Analyzer options, stored in the session (message thread only). Invalid values read as defaults. */
    AnalyzerSettings::Values getAnalyzerSettings() const;
    void setAnalyzerSettings (const AnalyzerSettings::Values& values);

private:
    //==============================================================================
    void pushParametersToBands() noexcept;

    /** Raw parameter values of one band, read on the audio thread. */
    struct BandParameters
    {
        std::atomic<float>* frequency = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* slope = nullptr;
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* channel = nullptr;
        std::array<std::atomic<float>*, std::size (Parameters::dynamicFields)> dynamics {};
    };

    BandSettings readBandSettings (size_t index) const noexcept;

    int requestedTaps() const noexcept;
    void updateLatency();
    void readPhaseModeFromState();
    void processLinearPhaseMode (juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* sidechain) noexcept;

    /** Output gain x Auto Gain offset x polarity, as one linear gain. */
    double targetOutputGain() const noexcept;
    void applyOutputGain (juce::AudioBuffer<float>& buffer) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    std::array<std::atomic<bool>, 16> bandInUse {};
    AnalyzerFifo preFifo, postFifo, sidechainFifo;
    std::atomic<bool> sidechainTapActive { false };
    std::atomic<bool> analyzerActive { false };
    std::array<BandParameters, 16> bandParameters;
    std::array<EqBand, 16> bands;

    std::atomic<float>* outputGainDb = nullptr;
    std::atomic<float>* autoGainOn = nullptr;
    std::atomic<float>* invertOn = nullptr;

    /** One ramp for gain, Auto Gain and polarity; a polarity switch ramps through zero. */
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> outputGain;

    AutoGainUpdater autoGainUpdater { parameters, bandInUse };

    // Phase mode (M8). Requested on the message thread; the audio thread switches over with a
    // fade out, a filter load and a fade in. Settings are pushed per block into pushedBands.
    std::atomic<bool> requestedLinear { false };
    std::atomic<int> requestedLength { 0 };
    std::atomic<double> currentSampleRate { 48000.0 };
    std::array<BandSettings, 16> pushedBands {};
    std::array<bool, 16> bandRan {};
    bool activeLinear = false;
    int activeTaps = LinearPhaseDesigner::tapCounts[0];
    juce::int64 samplesFed = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> modeFade;
    std::atomic<bool> phaseSettled { true };
    LinearPhaseEngine linearPhaseEngine;

    // Spectral dynamics (M9g): active while any band is spectral; switching fades like a phase change.
    SpectralDynamicsEngine spectralEngine;
    bool activeSpectral = false;
    juce::int64 spectralFed = 0;
    void timerCallback() override { refreshLatency(); }
    LinearPhaseUpdater linearPhaseUpdater { linearPhaseEngine, [this]
    {
        return LinearPhaseUpdater::Request { requestedLinear.load(), requestedTaps(), currentSampleRate.load(), getBandSettings() };
    } };
    std::unique_ptr<PresetManager> presetManager;
    AbComparison abComparison { *this };
    UndoHistory undoHistory { *this };
    MatchSession matchSession { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQAudioProcessor)
};
