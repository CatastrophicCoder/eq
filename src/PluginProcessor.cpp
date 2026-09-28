#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "dsp/CutSlope.h"

//==============================================================================
ParametricEQAudioProcessor::ParametricEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "ParametricEQ", Parameters::createLayout())
{
    static_assert (std::tuple_size_v<decltype (bands)> == Parameters::numBands);

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        auto& p = bandParameters[static_cast<size_t> (band - 1)];
        p.frequency = parameters.getRawParameterValue (Parameters::id (band, "freq"));
        p.gain      = parameters.getRawParameterValue (Parameters::id (band, "gain"));
        p.q         = parameters.getRawParameterValue (Parameters::id (band, "q"));
        p.type      = parameters.getRawParameterValue (Parameters::id (band, "type"));
        p.slope     = parameters.getRawParameterValue (Parameters::id (band, "slope"));
        p.enabled   = parameters.getRawParameterValue (Parameters::id (band, "enabled"));
        jassert (p.frequency != nullptr && p.gain != nullptr && p.q != nullptr
                 && p.type != nullptr && p.slope != nullptr && p.enabled != nullptr);
    }
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

    pushParametersToBands();

    for (auto& band : bands)
        band.prepare (sampleRate, getTotalNumOutputChannels());
}

void ParametricEQAudioProcessor::pushParametersToBands() noexcept
{
    for (size_t i = 0; i < bands.size(); ++i)
    {
        const auto& p = bandParameters[i];

        BandSettings settings;
        settings.type = static_cast<FilterType> (juce::jlimit (0, FilterTypes::count - 1, juce::roundToInt (p.type->load())));
        settings.frequencyHz = p.frequency->load();
        settings.gainDb = p.gain->load();
        settings.q = p.q->load();
        settings.slopeIndex = juce::jlimit (0, CutSlope::count - 1, juce::roundToInt (p.slope->load()));
        settings.enabled = p.enabled->load() >= 0.5f;

        bands[i].setTargets (settings);
    }
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

    pushParametersToBands();

    for (auto& band : bands)
        band.process (buffer);
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

    // Version 1 (M1) had a single bell on band 1 and no type or enable parameters.
    if (version == 1)
    {
        auto setParameter = [this] (const juce::String& id, float value)
        {
            if (auto* p = parameters.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
        };

        setParameter (Parameters::id (1, "type"), static_cast<float> (FilterType::bell));
        setParameter (Parameters::id (1, "enabled"), 1.0f);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParametricEQAudioProcessor();
}
