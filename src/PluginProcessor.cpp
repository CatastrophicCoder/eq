#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "ui/FrequencyAxis.h"
#include "dsp/CutSlope.h"

//==============================================================================
ParametricEQAudioProcessor::ParametricEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
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
        p.channel   = parameters.getRawParameterValue (Parameters::id (band, "channel"));
        jassert (p.frequency != nullptr && p.gain != nullptr && p.q != nullptr
                 && p.type != nullptr && p.slope != nullptr && p.enabled != nullptr && p.channel != nullptr);

        for (size_t f = 0; f < p.dynamics.size(); ++f)
        {
            p.dynamics[f] = parameters.getRawParameterValue (Parameters::id (band, Parameters::dynamicFields[f]));
            jassert (p.dynamics[f] != nullptr);
        }
    }

    // Analyzer taps, allocated once: prepareToPlay may run while the editor is reading them.
    preFifo.prepare (2, analyzerFifoCapacity);
    postFifo.prepare (2, analyzerFifoCapacity);
    sidechainFifo.prepare (2, analyzerFifoCapacity);   // EQ Match reference (M9e)

    outputGainDb = parameters.getRawParameterValue (Parameters::outputGain);
    autoGainOn = parameters.getRawParameterValue (Parameters::autoGain);
    invertOn = parameters.getRawParameterValue (Parameters::outputInvert);
    jassert (outputGainDb != nullptr && autoGainOn != nullptr && invertOn != nullptr);

    presetManager = std::make_unique<PresetManager> (*this, PresetManager::defaultUserFolder());

    // Started last: its request reads the band parameters set up above.
    linearPhaseUpdater.start();
    undoHistory.attach();
    startTimerHz (10);   // reports spectral latency changes from the message thread (M9g)
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
        band.prepare (sampleRate, getMainBusNumOutputChannels());
    bandRan.fill (true);

    // Phase mode: start directly in the requested mode. A fresh stream has a silent past, so the
    // convolution's empty history is correct and no fade-in wait is needed.
    currentSampleRate = sampleRate;
    activeLinear = requestedLinear.load();
    activeTaps = requestedTaps();
    if (activeLinear)
    {
        LinearPhaseDesigner designer;
        LinearPhaseDesigner::Result result;
        designer.design (getBandSettings(), sampleRate, activeTaps, result);
        linearPhaseEngine.submit (result);   // if the updater's filter is pending instead, that one loads
    }
    linearPhaseEngine.prepare();
    samplesFed = activeTaps;

    spectralEngine.prepare (sampleRate);
    activeSpectral = SpectralDynamicsEngine::anySpectral (pushedBands);
    spectralFed = SpectralDynamicsEngine::latencySamples;   // a fresh stream: silent past
    updateLatency();
    modeFade.reset (sampleRate, EqBand::crossfadeSeconds);
    modeFade.setCurrentAndTargetValue (1.0f);

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
        pushedBands[i] = readBandSettings (i);

        // A spectral band's moving part runs in the spectral engine; its filter keeps the static gain.
        auto target = pushedBands[i];
        if (target.isSpectral())
            target.dynamics.on = false;
        bands[i].setTargets (target);
    }
}

BandSettings ParametricEQAudioProcessor::readBandSettings (size_t index) const noexcept
{
    const auto& p = bandParameters[index];
    auto s = Parameters::toBandSettings (p.type->load(), p.frequency->load(), p.gain->load(), p.q->load(),
                                         p.slope->load(), p.enabled->load(),
                                         bandInUse[index].load (std::memory_order_relaxed), p.channel->load());

    std::array<float, std::size (Parameters::dynamicFields)> raw {};
    for (size_t f = 0; f < raw.size(); ++f)
        raw[f] = p.dynamics[f]->load();

    s.dynamics = Parameters::toDynamics (raw);
    return s;
}

void ParametricEQAudioProcessor::releaseResources()
{
}

void ParametricEQAudioProcessor::reset()
{
    for (auto& band : bands)
        band.reset();

    // Like a fresh stream: an empty convolution history stands for a silent past.
    linearPhaseEngine.reset();
    samplesFed = activeTaps;
    spectralEngine.reset();
    spectralFed = SpectralDynamicsEngine::latencySamples;

    outputGain.setCurrentAndTargetValue (targetOutputGain());
}

bool ParametricEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Main: stereo in and out. Side-chain (M7): disabled, mono or stereo.
    const auto sidechain = layouts.inputBuses.size() > 1 ? layouts.inputBuses[1] : juce::AudioChannelSet::disabled();

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
        && (sidechain.isDisabled() || sidechain == juce::AudioChannelSet::mono()
            || sidechain == juce::AudioChannelSet::stereo());
}

void ParametricEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // The main bus is processed in place; the side-chain (when enabled) only feeds the detectors.
    auto main = getBusBuffer (buffer, false, 0);

    // Clear any outputs that have no matching input, since their contents are not guaranteed.
    for (auto i = getMainBusNumInputChannels(); i < main.getNumChannels(); ++i)
        main.clear (i, 0, main.getNumSamples());

    juce::AudioBuffer<float> sidechain;
    const auto* sidechainBus = getBus (true, 1);
    // Only when the buffer really carries the side-chain channels (a host always passes them all).
    const auto hasSidechain = sidechainBus != nullptr && sidechainBus->isEnabled()
                           && sidechainBus->getNumberOfChannels() > 0
                           && buffer.getNumChannels() >= getTotalNumInputChannels();
    if (hasSidechain)
        sidechain = getBusBuffer (buffer, true, 1);   // refers to the host's data, no allocation

    pushParametersToBands();

    // EQ Match learning from the side-chain (M9e): a mono side-chain feeds both tap channels.
    if (hasSidechain && sidechainTapActive.load (std::memory_order_relaxed))
    {
        const float* channels[] { sidechain.getReadPointer (0), sidechain.getReadPointer (std::min (1, sidechain.getNumChannels() - 1)) };
        sidechainFifo.push (channels, 2, sidechain.getNumSamples());
    }

    const auto tap = analyzerActive.load (std::memory_order_relaxed);
    const auto tapChannels = std::min (2, main.getNumChannels());

    if (tap)
        preFifo.push (main.getArrayOfReadPointers(), tapChannels, main.getNumSamples());

    processLinearPhaseMode (main, hasSidechain ? &sidechain : nullptr);

    applyOutputGain (main);

    if (tap)
        postFifo.push (main.getArrayOfReadPointers(), tapChannels, main.getNumSamples());
}

void ParametricEQAudioProcessor::processLinearPhaseMode (juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* sidechain) noexcept
{
    const auto numSamples = main.getNumSamples();
    const auto wantLinear = requestedLinear.load (std::memory_order_relaxed);
    const auto wantTaps = requestedTaps();
    const auto wantSpectral = SpectralDynamicsEngine::anySpectral (pushedBands);
    const auto differs = activeLinear != wantLinear || (wantLinear && activeTaps != wantTaps) || activeSpectral != wantSpectral;

    // Ready: the filter in use has the active length and a full input history. Checked before
    // processing: the engine swaps filters at the end of a block, so the new filter is heard from
    // the next block on, and a fade-in must not start on a block still made by the old one.
    // The spectral engine likewise needs one full frame of history.
    const auto ready = (! activeLinear || (linearPhaseEngine.getCurrentTaps() == activeTaps && samplesFed >= activeTaps))
                    && (! activeSpectral || spectralFed >= SpectralDynamicsEngine::latencySamples);

    // Linear phase: the static bands are the FIR; dynamic bands run after it (decision 2026-09-29).
    if (activeLinear)
    {
        linearPhaseEngine.process (main);
        samplesFed += numSamples;
    }

    for (size_t i = 0; i < bands.size(); ++i)
    {
        const auto runIir = ! activeLinear || pushedBands[i].isDynamic();
        if (runIir)
        {
            if (! bandRan[i])
                bands[i].reset();   // skipped until now: start from fresh state, not stale history
            bands[i].process (main, sidechain);
        }
        bandRan[i] = runIir;
    }

    // Spectral dynamics after the bands, on the whole signal, so everything is delayed alike.
    if (activeSpectral)
    {
        spectralEngine.setBands (pushedBands);
        spectralEngine.process (main, sidechain);
        spectralFed += numSamples;
    }

    // Fade out to switch, and stay silent until a newly started filter has its full history.
    modeFade.setTargetValue (differs || ! ready ? 0.0f : 1.0f);

    if (modeFade.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto g = modeFade.getNextValue();
            for (int ch = 0; ch < main.getNumChannels(); ++ch)
                main.getWritePointer (ch)[i] *= g;
        }
    }
    else if (modeFade.getCurrentValue() <= 0.0f)
    {
        main.clear();
    }

    if (differs && ! modeFade.isSmoothing() && modeFade.getCurrentValue() <= 0.0f)
    {
        activeLinear = wantLinear;
        activeTaps = wantTaps;
        linearPhaseEngine.reset();
        samplesFed = 0;
        activeSpectral = wantSpectral;
        spectralEngine.reset();
        spectralFed = 0;
        for (auto& band : bands)
            band.reset();
        bandRan.fill (true);
    }

    phaseSettled.store (! differs && ready && ! modeFade.isSmoothing() && modeFade.getCurrentValue() >= 1.0f,
                        std::memory_order_relaxed);
}

float ParametricEQAudioProcessor::getLiveGainChangeDb (int band, int channel) const noexcept
{
    if (band < 1 || band > static_cast<int> (bands.size()))
        return 0.0f;

    return bands[static_cast<size_t> (band - 1)].getLiveGainChangeDb (channel);
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

    UndoHistory::ScopedTransaction step (undoHistory);   // a sound edit (M9b), unless inside a larger one
    parameters.state.setProperty (inUseProperty (band), inUse, nullptr);
    bandInUse[static_cast<size_t> (band - 1)].store (inUse, std::memory_order_relaxed);
}

std::array<BandSettings, 16> ParametricEQAudioProcessor::getBandSettings() const
{
    std::array<BandSettings, 16> result;

    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i] = readBandSettings (i);
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

namespace
{
    const juce::Identifier presetNameProperty { "presetName" };
    const juce::Identifier presetFactoryProperty { "presetFactory" };
}

juce::String ParametricEQAudioProcessor::getStoredPresetName() const
{
    return parameters.state.getProperty (presetNameProperty).toString();
}

bool ParametricEQAudioProcessor::isStoredPresetFactory() const
{
    return static_cast<bool> (parameters.state.getProperty (presetFactoryProperty, false));
}

void ParametricEQAudioProcessor::setStoredPreset (const juce::String& name, bool isFactory)
{
    parameters.state.setProperty (presetNameProperty, name, nullptr);
    parameters.state.setProperty (presetFactoryProperty, isFactory, nullptr);
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
    xml->addChildElement (abComparison.toState().createXml().release());   // version 5 (M9a)
    xml->addChildElement (matchSession.toState().createXml().release());   // version 6 (M9e)
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

    // A session load is not an undo step and starts a fresh history (M9b).
    UndoHistory::ScopedSuspend suspendUndo (undoHistory);
    undoHistory.clear();

    // The A/B element (version 5) is kept apart: it is not part of the setting itself.
    auto tree = juce::ValueTree::fromXml (*xml);
    const auto abState = tree.getChildWithName (AbComparison::stateTag);
    if (abState.isValid())
        tree.removeChild (abState, nullptr);
    const auto matchState = tree.getChildWithName (MatchSession::stateTag);   // version 6 (M9e)
    if (matchState.isValid())
        tree.removeChild (matchState, nullptr);
    tree.removeProperty ("stateVersion", nullptr);

    parameters.replaceState (tree);

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

    readPhaseModeFromState();   // version 4 (M8); older states read as Zero latency
    presetManager->restoreFromSession();
    abComparison.fromState (abState);   // version 5 (M9a); older states: both slots equal
    matchSession.fromState (matchState);   // version 6 (M9e); older states: nothing learned
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParametricEQAudioProcessor();
}


//==============================================================================
namespace
{
    const juce::Identifier linearPhaseProperty { "linearPhase" };
    const juce::Identifier linearPhaseLengthProperty { "linearPhaseLength" };
}

int ParametricEQAudioProcessor::requestedTaps() const noexcept
{
    const auto index = juce::jlimit (0, static_cast<int> (LinearPhaseDesigner::tapCounts.size()) - 1,
                                     requestedLength.load (std::memory_order_relaxed));
    return LinearPhaseDesigner::tapCounts[static_cast<size_t> (index)];
}

bool ParametricEQAudioProcessor::isLinearPhase() const
{
    return requestedLinear.load();
}

void ParametricEQAudioProcessor::setLinearPhase (bool shouldBeLinear)
{
    UndoHistory::ScopedTransaction step (undoHistory);
    parameters.state.setProperty (linearPhaseProperty, shouldBeLinear, nullptr);
    requestedLinear = shouldBeLinear;
    updateLatency();
}

int ParametricEQAudioProcessor::getLinearPhaseLength() const
{
    return requestedLength.load();
}

void ParametricEQAudioProcessor::setLinearPhaseLength (int index)
{
    if (index < 0 || index >= static_cast<int> (LinearPhaseDesigner::tapCounts.size()))
        return;

    UndoHistory::ScopedTransaction step (undoHistory);
    parameters.state.setProperty (linearPhaseLengthProperty, index, nullptr);
    requestedLength = index;
    updateLatency();
}

void ParametricEQAudioProcessor::readPhaseModeFromState()
{
    // Missing (sessions before M8) or invalid values read as Zero latency and the shortest length.
    const auto length = static_cast<int> (parameters.state.getProperty (linearPhaseLengthProperty, 0));
    requestedLinear = static_cast<bool> (parameters.state.getProperty (linearPhaseProperty, false));
    requestedLength = juce::isPositiveAndBelow (length, static_cast<int> (LinearPhaseDesigner::tapCounts.size())) ? length : 0;
    updateLatency();
}

void ParametricEQAudioProcessor::updateLatency()
{
    const auto spectral = SpectralDynamicsEngine::anySpectral (getBandSettings());
    setLatencySamples ((requestedLinear.load() ? LinearPhaseEngine::latencyFor (requestedTaps()) : 0)
                       + (spectral ? SpectralDynamicsEngine::latencySamples : 0));
}

bool ParametricEQAudioProcessor::isPhaseModeSettled() const noexcept
{
    return phaseSettled.load (std::memory_order_relaxed);
}

int ParametricEQAudioProcessor::getLinearPhaseSwapCount() const noexcept
{
    return linearPhaseEngine.getSwapCount();
}

bool ParametricEQAudioProcessor::isSidechainConnected() const
{
    const auto* bus = getBus (true, 1);
    return bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0;
}

ParametricEQAudioProcessor::~ParametricEQAudioProcessor()
{
    stopTimer();
}

void ParametricEQAudioProcessor::refreshLatency()
{
    updateLatency();
}
