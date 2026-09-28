#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "ui/FrequencyAxis.h"
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

    // Analyzer taps, allocated once: prepareToPlay may run while the editor is reading them.
    preFifo.prepare (2, analyzerFifoCapacity);
    postFifo.prepare (2, analyzerFifoCapacity);

    outputGainDb = parameters.getRawParameterValue (Parameters::outputGain);
    autoGainOn = parameters.getRawParameterValue (Parameters::autoGain);
    invertOn = parameters.getRawParameterValue (Parameters::outputInvert);
    jassert (outputGainDb != nullptr && autoGainOn != nullptr && invertOn != nullptr);
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

    autoGainUpdater.setSampleRate (sampleRate);

    outputGain.reset (sampleRate, EqBand::rampSeconds);
    outputGain.setCurrentAndTargetValue (targetOutputGain());
}

double ParametricEQAudioProcessor::targetOutputGain() const noexcept
{
    const auto autoGainDb = autoGainOn->load() >= 0.5f ? static_cast<double> (autoGainUpdater.getOffsetDb()) : 0.0;
    const auto polarity = invertOn->load() >= 0.5f ? -1.0 : 1.0;
    return polarity * juce::Decibels::decibelsToGain (static_cast<double> (outputGainDb->load()) + autoGainDb, -1000.0);
}

void ParametricEQAudioProcessor::applyOutputGain (juce::AudioBuffer<float>& buffer) noexcept
{
    outputGain.setTargetValue (targetOutputGain());

    if (! outputGain.isSmoothing())
    {
        // Exactly unity: leave the samples untouched, so a neutral instance stays bit-exact.
        const auto gain = outputGain.getCurrentValue();
        if (juce::exactlyEqual (gain, 1.0))
            return;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            juce::FloatVectorOperations::multiply (buffer.getWritePointer (ch), static_cast<float> (gain), buffer.getNumSamples());

        return;
    }

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto gain = static_cast<float> (outputGain.getNextValue());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[i] *= gain;
    }
}

void ParametricEQAudioProcessor::pushParametersToBands() noexcept
{
    for (size_t i = 0; i < bands.size(); ++i)
    {
        const auto& p = bandParameters[i];
        bands[i].setTargets (Parameters::toBandSettings (p.type->load(), p.frequency->load(), p.gain->load(),
                                                         p.q->load(), p.slope->load(), p.enabled->load(),
                                                         bandInUse[i].load (std::memory_order_relaxed)));
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

    const auto tap = analyzerActive.load (std::memory_order_relaxed);
    const auto tapChannels = std::min (2, buffer.getNumChannels());

    if (tap)
        preFifo.push (buffer.getArrayOfReadPointers(), tapChannels, buffer.getNumSamples());

    for (auto& band : bands)
        band.process (buffer);

    applyOutputGain (buffer);

    if (tap)
        postFifo.push (buffer.getArrayOfReadPointers(), tapChannels, buffer.getNumSamples());
}

float ParametricEQAudioProcessor::getAutoGainOffsetDb() const noexcept
{
    return autoGainUpdater.getOffsetDb();
}

namespace
{
    const juce::Identifier displayRangeProperty { "displayRangeDb" };
}

double ParametricEQAudioProcessor::getDisplayRangeDb() const
{
    // Stored with the session; anything missing or not one of the four ranges reads as the default.
    const auto stored = static_cast<double> (parameters.state.getProperty (displayRangeProperty, FrequencyAxis::defaultRangeDb));
    return FrequencyAxis::isValidRange (stored) ? stored : FrequencyAxis::defaultRangeDb;
}

void ParametricEQAudioProcessor::setDisplayRangeDb (double rangeDb)
{
    if (FrequencyAxis::isValidRange (rangeDb))
        parameters.state.setProperty (displayRangeProperty, rangeDb, nullptr);
}

namespace
{
    juce::Identifier inUseProperty (int band)
    {
        return juce::Identifier ("band" + juce::String (band) + "_used");
    }
}

bool ParametricEQAudioProcessor::isBandInUse (int band) const noexcept
{
    return band >= 1 && band <= static_cast<int> (bandInUse.size())
        && bandInUse[static_cast<size_t> (band - 1)].load (std::memory_order_relaxed);
}

void ParametricEQAudioProcessor::setBandInUse (int band, bool inUse)
{
    if (band < 1 || band > static_cast<int> (bandInUse.size()))
        return;

    parameters.state.setProperty (inUseProperty (band), inUse, nullptr);
    bandInUse[static_cast<size_t> (band - 1)].store (inUse, std::memory_order_relaxed);
}

std::array<BandSettings, 16> ParametricEQAudioProcessor::getBandSettings() const
{
    std::array<BandSettings, 16> result;

    for (size_t i = 0; i < result.size(); ++i)
    {
        const auto& p = bandParameters[i];
        result[i] = Parameters::toBandSettings (p.type->load(), p.frequency->load(), p.gain->load(), p.q->load(),
                                                p.slope->load(), p.enabled->load(), bandInUse[i].load (std::memory_order_relaxed));
    }

    return result;
}

void ParametricEQAudioProcessor::setAnalyzerActive (bool shouldBeActive) noexcept
{
    analyzerActive.store (shouldBeActive, std::memory_order_relaxed);
}

namespace
{
    const juce::Identifier analyzerModeProperty { "analyzerMode" };
    const juce::Identifier analyzerResolutionProperty { "analyzerResolution" };
    const juce::Identifier analyzerSpeedProperty { "analyzerSpeed" };
    const juce::Identifier analyzerRangeProperty { "analyzerRange" };

    int validIndex (const juce::var& stored, int count, int fallback)
    {
        const auto v = static_cast<int> (stored);
        return stored.isVoid() || v < 0 || v >= count ? fallback : v;
    }
}

AnalyzerSettings::Values ParametricEQAudioProcessor::getAnalyzerSettings() const
{
    const AnalyzerSettings::Values defaults;
    const auto& state = parameters.state;

    return { validIndex (state.getProperty (analyzerModeProperty), static_cast<int> (AnalyzerSettings::modeNames.size()), defaults.mode),
             validIndex (state.getProperty (analyzerResolutionProperty), static_cast<int> (AnalyzerSettings::fftOrders.size()), defaults.resolution),
             validIndex (state.getProperty (analyzerSpeedProperty), static_cast<int> (AnalyzerSettings::releaseDbPerSecond.size()), defaults.speed),
             validIndex (state.getProperty (analyzerRangeProperty), static_cast<int> (AnalyzerSettings::ranges.size()), defaults.range) };
}

void ParametricEQAudioProcessor::setAnalyzerSettings (const AnalyzerSettings::Values& values)
{
    // Out-of-range values are stored as given and read back as the defaults.
    parameters.state.setProperty (analyzerModeProperty, values.mode, nullptr);
    parameters.state.setProperty (analyzerResolutionProperty, values.resolution, nullptr);
    parameters.state.setProperty (analyzerSpeedProperty, values.speed, nullptr);
    parameters.state.setProperty (analyzerRangeProperty, values.range, nullptr);
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

    // Version 3 stores which bands are in use; before that, a band was in use when it was enabled.
    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        const auto inUse = version >= 3
                             ? static_cast<bool> (parameters.state.getProperty (inUseProperty (band), false))
                             : parameters.getRawParameterValue (Parameters::id (band, "enabled"))->load() >= 0.5f;
        setBandInUse (band, inUse);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParametricEQAudioProcessor();
}
