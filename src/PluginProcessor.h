#pragma once

#include "AutoGainUpdater.h"
#include "presets/PresetManager.h"
#include "dsp/AnalyzerFifo.h"
#include "dsp/EqBand.h"
#include "ui/AnalyzerSettings.h"

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
class ParametricEQAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    ParametricEQAudioProcessor();
    ~ParametricEQAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

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
    static constexpr int stateVersion = 3;

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

    //==============================================================================
    /** Analyzer taps (M5): stereo input (pre) and output (post), pushed from processBlock
        only while an editor is open. The editor is the single consumer.
    */
    void setAnalyzerActive (bool shouldBeActive) noexcept;
    bool isAnalyzerActive() const noexcept { return analyzerActive.load (std::memory_order_relaxed); }
    AnalyzerFifo& getPreFifo() noexcept  { return preFifo; }
    AnalyzerFifo& getPostFifo() noexcept { return postFifo; }
    static constexpr int analyzerFifoCapacity = 32768;

    /** Presets (M6b). Message thread only. */
    PresetManager& getPresetManager() noexcept { return *presetManager; }

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
    };

    /** Output gain x Auto Gain offset x polarity, as one linear gain. */
    double targetOutputGain() const noexcept;
    void applyOutputGain (juce::AudioBuffer<float>& buffer) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    std::array<std::atomic<bool>, 16> bandInUse {};
    AnalyzerFifo preFifo, postFifo;
    std::atomic<bool> analyzerActive { false };
    std::array<BandParameters, 16> bandParameters;
    std::array<EqBand, 16> bands;

    std::atomic<float>* outputGainDb = nullptr;
    std::atomic<float>* autoGainOn = nullptr;
    std::atomic<float>* invertOn = nullptr;

    /** One ramp for gain, Auto Gain and polarity; a polarity switch ramps through zero. */
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> outputGain;

    AutoGainUpdater autoGainUpdater { parameters, bandInUse };
    std::unique_ptr<PresetManager> presetManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQAudioProcessor)
};
