#include "MatchSession.h"

#include "BandParameterWriter.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "dsp/BandDesign.h"
#include "dsp/CurveFitter.h"
#include "dsp/MatchCurve.h"

#include <cmath>

MatchSession::MatchSession (ParametricEQAudioProcessor& p) : processor (p)
{
    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    reference.prepare (rate);
    current.prepare (rate);
}

bool MatchSession::usesSidechain() const
{
    return processor.isSidechainConnected();
}

void MatchSession::startLearning (Learning what)
{
    stopLearning();
    if (what == Learning::none || (what == Learning::both && ! usesSidechain()))
        return;

    // A new pass starts from nothing, at the current sample rate.
    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    if (what == Learning::reference || what == Learning::both)
        reference.prepare (rate);
    if (what == Learning::current || what == Learning::both)
        current.prepare (rate);

    learning = what;
    processor.setSidechainTapActive (what == Learning::both);
}

void MatchSession::stopLearning()
{
    learning = Learning::none;
    processor.setSidechainTapActive (false);
}

void MatchSession::addInputSamples (const float* samples, int numSamples)
{
    if (learning == Learning::reference)
        reference.addSamples (samples, numSamples);
    else if (learning == Learning::current || learning == Learning::both)
        current.addSamples (samples, numSamples);
}

void MatchSession::addSidechainSamples (const float* samples, int numSamples)
{
    if (learning == Learning::both)
        reference.addSamples (samples, numSamples);
}

bool MatchSession::canApply() const noexcept
{
    return reference.hasData() && current.hasData()
        && reference.getSeconds() >= minimumSeconds && current.getSeconds() >= minimumSeconds;
}

std::vector<double> MatchSession::curveDb (const std::vector<double>& frequenciesHz) const
{
    if (! canApply())
        return std::vector<double> (frequenciesHz.size(), 0.0);

    return MatchCurve::compute (frequenciesHz, reference.levelsDb (frequenciesHz), current.levelsDb (frequenciesHz),
                                amount, smoothing);
}

int MatchSession::freeSlots() const
{
    int free = 0;
    for (int b = 1; b <= Parameters::numBands; ++b)
        free += processor.isBandInUse (b) ? 0 : 1;
    return free;
}

void MatchSession::apply (ApplyMode mode)
{
    if (! canApply())
        return;

    const auto bands = processor.getBandSettings();
    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    // Replace all: every slot. Keep existing: the free slots, fitted to the match minus the kept bands.
    std::vector<int> slots;
    std::vector<BandSettings> kept;
    for (int b = 1; b <= Parameters::numBands; ++b)
    {
        const auto& s = bands[static_cast<size_t> (b - 1)];
        if (mode == ApplyMode::replaceAll || ! s.inUse)
            slots.push_back (b);
        else
            kept.push_back (s);
    }
    if (slots.empty())
        return;

    CurveFitter::Problem problem;
    problem.sampleRate = rate;
    for (int i = 0; i < 256; ++i)
        problem.frequenciesHz.push_back (20.0 * std::pow (1000.0, i / 255.0));

    const auto match = curveDb (problem.frequenciesHz);
    for (size_t i = 0; i < problem.frequenciesHz.size(); ++i)
    {
        double keptDb = 0.0;
        for (const auto& k : kept)
            if (k.isActive())
                keptDb += BandDesign::design (k, rate).magnitudeDb (std::min (problem.frequenciesHz[i], 0.49 * rate), rate);
        problem.targetDb.push_back (match[i] - keptDb);
        problem.weights.push_back (1.0);
    }

    const auto fitted = CurveFitter::fit (problem, static_cast<int> (slots.size()));

    UndoHistory::ScopedTransaction step (processor.getUndoHistory());   // the whole match is one undo step
    BandParameterWriter writer (processor.getValueTreeState());
    for (auto b : slots)
        if (bands[static_cast<size_t> (b - 1)].inUse)
            processor.setBandInUse (b, false);

    for (size_t i = 0; i < fitted.size() && i < slots.size(); ++i)
    {
        writer.writeFittedBand (slots[i], fitted[i]);
        processor.setBandInUse (slots[i], true);
    }
}

juce::ValueTree MatchSession::toState() const { return juce::ValueTree (stateTag); }
void MatchSession::fromState (const juce::ValueTree&) {}
