#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

//==============================================================================
ParametricEQAudioProcessor::ParametricEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "ParametricEQ", Parameters::createLayout()),
      band1Freq (parameters.getRawParameterValue (Parameters::band1Freq)),
      band1Gain (parameters.getRawParameterValue (Parameters::band1Gain)),
      band1Q (parameters.getRawParameterValue (Parameters::band1Q))
{
    jassert (band1Freq != nullptr && band1Gain != nullptr && band1Q != nullptr);
}

//==============================================================================
const juce::String ParametricEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ParametricEQAudioProcessor::acceptsMidi() const   { return false; }
bool ParametricEQAudioProcessor::producesMidi() const  { return false; }
bool ParametricEQAudioProcessor::isMidiEffect() const  { return false; }

double ParametricEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

// Some hosts don't cope with 0 programs, so report one.
int ParametricEQAudioProcessor::getNumPrograms()                   { return 1; }
int ParametricEQAudioProcessor::getCurrentProgram()                { return 0; }
void ParametricEQAudioProcessor::setCurrentProgram (int)           {}
const juce::String ParametricEQAudioProcessor::getProgramName (int) { return {}; }
void ParametricEQAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void ParametricEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    pushParametersToBand();
    band1.prepare (sampleRate, getTotalNumOutputChannels());
}

void ParametricEQAudioProcessor::pushParametersToBand() noexcept
{
    band1.setTargets (band1Freq->load(), band1Gain->load(), band1Q->load());
}

void ParametricEQAudioProcessor::releaseResources()
{
}

bool ParametricEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void ParametricEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Clear any outputs that have no matching input, since their contents are not guaranteed.
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    pushParametersToBand();
    band1.process (buffer);
}

//==============================================================================
bool ParametricEQAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ParametricEQAudioProcessor::createEditor()
{
    return new ParametricEQAudioProcessorEditor (*this);
}

//==============================================================================
void ParametricEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto xml = parameters.copyState().createXml();
    xml->setAttribute ("stateVersion", stateVersion);
    copyXmlToBinary (*xml, destData);
}

void ParametricEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    // Ignore anything that is not our state, or is from an unknown (newer) format.
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    const auto version = xml->getIntAttribute ("stateVersion", 0);
    if (version < 1 || version > stateVersion)
        return;

    parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParametricEQAudioProcessor();
}
