#pragma once

#include "dsp/EqBand.h"

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
    static constexpr int stateVersion = 2;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }

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
    };

    juce::AudioProcessorValueTreeState parameters;
    std::array<BandParameters, 16> bandParameters;
    std::array<EqBand, 16> bands;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQAudioProcessor)
};
