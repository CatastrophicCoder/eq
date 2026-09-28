#include "EqBand.h"

#include "BandDesign.h"
#include "MatchedBandpassDesign.h"
#include "MatchedHighpassDesign.h"
#include "MatchedLowpassDesign.h"

#include <algorithm>
#include <cmath>
#include <limits>

EqBand::EqBand() = default;

void EqBand::prepare (double newSampleRate, int newNumChannels)
{
    sampleRate = newSampleRate;
    numChannels = std::clamp (newNumChannels, 1, CascadeProcessor::maxChannels);
    fadeLength = std::max (1, static_cast<int> (std::ceil (crossfadeSeconds * sampleRate)));
    fadeRemaining = 0;

    frequency.reset (sampleRate, rampSeconds);
    gainDb.reset (sampleRate, rampSeconds);
    q.reset (sampleRate, rampSeconds);

    frequency.setCurrentAndTargetValue (target.frequencyHz);
    gainDb.setCurrentAndTargetValue (target.gainDb);
    q.setCurrentAndTargetValue (target.q);

    thresholdDb.reset (sampleRate, rampSeconds);
    rangeDb.reset (sampleRate, rampSeconds);
    ratio.reset (sampleRate, rampSeconds);
    thresholdDb.setCurrentAndTargetValue (target.dynamics.thresholdDb);
    rangeDb.setCurrentAndTargetValue (target.dynamics.rangeDb);
    ratio.setCurrentAndTargetValue (target.dynamics.ratio);

    for (auto& detector : detectors)
        detector.prepare (sampleRate);
    appliedAttackMs = appliedReleaseMs = -1.0;
    detectorFrequency = detectorQ = 0.0;

    active = 0;
    for (auto& slot : slots)
    {
        slot.discrete = target;
        slot.processor.reset();
    }

    updateCoefficients();
}

void EqBand::setTargets (const BandSettings& newTargets) noexcept
{
    target = newTargets;

    frequency.setTargetValue (target.frequencyHz);
    gainDb.setTargetValue (target.gainDb);
    q.setTargetValue (target.q);

    thresholdDb.setTargetValue (target.dynamics.thresholdDb);
    rangeDb.setTargetValue (target.dynamics.rangeDb);
    ratio.setTargetValue (target.dynamics.ratio);
}

void EqBand::reset() noexcept
{
    // With the state cleared there is nothing to fade from: adopt the target's
    // discrete settings directly.
    fadeRemaining = 0;
    slots[static_cast<size_t> (active)].discrete = target;
    updateCoefficients();

    for (auto& slot : slots)
        slot.processor.reset();

    // Detection starts again from silence on the next block.
    detecting = false;
    gainChange = {};
    for (auto& live : liveGainChangeDb)
        live.store (0.0f, std::memory_order_relaxed);
}

namespace
{
    /** One stereo frame through a band's filter, routed by its channel mode (decision 2026-09-28).
        Mid/Side encode M = (L+R)/2, S = (L-R)/2 and decode L = M+S, R = M-S around the filter;
        M and S use the filter state of channel 0. */
    inline void route (CascadeProcessor& filter, ChannelMode mode, bool twoChannels, double& l, double& r) noexcept
    {
        if (! twoChannels)
        {
            l = filter.processSample (0, l);
            return;
        }

        switch (mode)
        {
            case ChannelMode::stereo:
                l = filter.processSample (0, l);
                r = filter.processSample (1, r);
                break;

            case ChannelMode::left:
                l = filter.processSample (0, l);
                break;

            case ChannelMode::right:
                r = filter.processSample (1, r);
                break;

            case ChannelMode::mid:
            {
                const auto m = filter.processSample (0, 0.5 * (l + r));
                const auto sd = 0.5 * (l - r);
                l = m + sd;
                r = m - sd;
                break;
            }

            case ChannelMode::side:
            {
                const auto m = 0.5 * (l + r);
                const auto sd = filter.processSample (0, 0.5 * (l - r));
                l = m + sd;
                r = m - sd;
                break;
            }
        }
    }
}

void EqBand::process (juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain) noexcept
{
    const auto channels = std::min (buffer.getNumChannels(), numChannels);
    const auto twoChannels = channels >= 2;
    const auto numSamples = buffer.getNumSamples();
    auto* left = buffer.getWritePointer (0);
    auto* right = twoChannels ? buffer.getWritePointer (1) : nullptr;

    // Side-chain source for the detectors; a mono side-chain feeds both detection channels.
    const auto useSidechain = target.dynamics.sidechain && sidechain != nullptr
                           && sidechain->getNumChannels() > 0 && sidechain->getNumSamples() >= numSamples;
    const auto* scLeft = useSidechain ? sidechain->getReadPointer (0) : nullptr;
    const auto* scRight = useSidechain ? sidechain->getReadPointer (std::min (1, sidechain->getNumChannels() - 1)) : nullptr;

    for (int start = 0; start < numSamples;)
    {
        if (! isCrossfading() && ! target.hasSameDiscreteSettings (slots[static_cast<size_t> (active)].discrete))
            startCrossfade();

        const auto* dynamic = dynamicSlot();

        if (dynamic != nullptr && ! detecting)
        {
            // Dynamics just came on: the detectors start from silence.
            detecting = true;
            detectorFilter.reset();
            for (auto& detector : detectors)
                detector.reset();
            gainChange = {};
        }
        else if (dynamic == nullptr && detecting)
        {
            detecting = false;
            gainChange = {};
        }

        const auto length = std::min (detecting ? dynamicStep : subBlockSize, numSamples - start);

        if (frequency.isSmoothing() || gainDb.isSmoothing() || q.isSmoothing())
        {
            frequency.skip (length);
            gainDb.skip (length);
            q.skip (length);
            updateCoefficients();
        }

        thresholdDb.skip (length);
        rangeDb.skip (length);
        ratio.skip (length);

        const auto detectMode = dynamic != nullptr ? dynamic->discrete.channel : ChannelMode::stereo;

        if (detecting)
        {
            updateDetector (*dynamic);
            applyDynamicGain();
        }

        auto& current = slots[static_cast<size_t> (active)];

        if (! isCrossfading())
        {
            // A bypassed band (no sections) leaves the audio untouched: skip the loop.
            if (! detecting && current.processor.getCascade().numSections == 0)
            {
                start += length;
                continue;
            }

            for (int i = start; i < start + length; ++i)
            {
                auto l = static_cast<double> (left[i]);
                auto r = twoChannels ? static_cast<double> (right[i]) : 0.0;

                if (detecting)
                    detect (useSidechain ? scLeft[i] : l, useSidechain ? scRight[i] : r, detectMode, twoChannels);

                route (current.processor, current.discrete.channel, twoChannels, l, r);
                left[i] = static_cast<float> (l);
                if (twoChannels)
                    right[i] = static_cast<float> (r);
            }

            start += length;
            continue;
        }

        // Linear crossfade from the current to the incoming filter, each with its own routing.
        auto& incoming = slots[static_cast<size_t> (1 - active)];
        const auto done = fadeLength - fadeRemaining;

        for (int i = 0; i < length; ++i)
        {
            const auto n = start + i;
            const auto inL = static_cast<double> (left[n]);
            const auto inR = twoChannels ? static_cast<double> (right[n]) : 0.0;

            if (detecting)
                detect (useSidechain ? scLeft[n] : inL, useSidechain ? scRight[n] : inR, detectMode, twoChannels);

            auto aL = inL, aR = inR, bL = inL, bR = inR;
            route (current.processor, current.discrete.channel, twoChannels, aL, aR);
            route (incoming.processor, incoming.discrete.channel, twoChannels, bL, bR);

            const auto g = std::min (1.0, static_cast<double> (done + i + 1) / fadeLength);
            left[n] = static_cast<float> ((1.0 - g) * aL + g * bL);
            if (twoChannels)
                right[n] = static_cast<float> ((1.0 - g) * aR + g * bR);
        }

        fadeRemaining -= length;

        if (fadeRemaining <= 0)
        {
            fadeRemaining = 0;
            active = 1 - active;
        }

        start += length;
    }

    for (size_t ch = 0; ch < liveGainChangeDb.size(); ++ch)
        liveGainChangeDb[ch].store (static_cast<float> (gainChange[ch]), std::memory_order_relaxed);
}

const EqBand::Slot* EqBand::dynamicSlot() const noexcept
{
    const auto& current = slots[static_cast<size_t> (active)];
    if (current.discrete.isDynamic())
        return &current;

    const auto& incoming = slots[static_cast<size_t> (1 - active)];
    return isCrossfading() && incoming.discrete.isDynamic() ? &incoming : nullptr;
}

void EqBand::updateDetector (const Slot& slot) noexcept
{
    const auto& d = target.dynamics;

    if (! juce::exactlyEqual (d.attackMs, appliedAttackMs) || ! juce::exactlyEqual (d.releaseMs, appliedReleaseMs))
    {
        for (auto& detector : detectors)
            detector.setTimes (d.attackMs, d.releaseMs);
        appliedAttackMs = d.attackMs;
        appliedReleaseMs = d.releaseMs;
    }

    for (auto& detector : detectors)
        detector.setMode (d.detector);

    // Detector filter over the band's region (decision 2026-09-28): band pass (f, Q) for a
    // bell, lowpass/highpass (f, 0.71) for a low/high shelf. Redesigned only when it moves.
    const auto type = slot.discrete.type;
    const auto f = std::min (frequency.getCurrentValue(), BandDesign::maxFrequencyRatio * sampleRate);
    const auto bandQ = q.getCurrentValue();

    if (type == detectorType && juce::exactlyEqual (f, detectorFrequency) && juce::exactlyEqual (bandQ, detectorQ))
        return;

    SectionCascade cascade;
    if (type == FilterType::lowShelf)
        cascade.add (MatchedLowpassDesign::design (f, detectorShelfQ, sampleRate));
    else if (type == FilterType::highShelf)
        cascade.add (MatchedHighpassDesign::design (f, detectorShelfQ, sampleRate));
    else
        cascade.add (MatchedBandpassDesign::design (f, bandQ, sampleRate));

    detectorFilter.setCoefficients (cascade);
    detectorType = type;
    detectorFrequency = f;
    detectorQ = bandQ;
}

void EqBand::detect (double l, double r, ChannelMode mode, bool twoChannels) noexcept
{
    auto feed = [this] (int ch, double x) { detectors[static_cast<size_t> (ch)].process (detectorFilter.processSample (ch, x)); };

    if (! twoChannels)
    {
        feed (0, l);
        return;
    }

    switch (mode)
    {
        case ChannelMode::stereo: feed (0, l); feed (1, r); break;
        case ChannelMode::left:   feed (0, l); break;
        case ChannelMode::right:  feed (0, r); break;
        case ChannelMode::mid:    feed (0, 0.5 * (l + r)); break;
        case ChannelMode::side:   feed (0, 0.5 * (l - r)); break;
    }
}

void EqBand::applyDynamicGain() noexcept
{
    const auto* dynamic = dynamicSlot();
    const auto perChannel = dynamic->discrete.channel == ChannelMode::stereo && numChannels >= 2;
    const auto& d = target.dynamics;

    auto change = [&] (int ch)
    {
        return DynamicGainLaw::gainChangeDb (d.mode, detectors[static_cast<size_t> (ch)].getLevelDb(),
                                             thresholdDb.getCurrentValue(), rangeDb.getCurrentValue(), ratio.getCurrentValue());
    };

    // Single-channel modes detect on channel 0 and apply the change wherever the band acts.
    gainChange[0] = change (0);
    gainChange[1] = perChannel ? change (1) : gainChange[0];

    for (int i = 0; i < 2; ++i)
    {
        auto& slot = slots[static_cast<size_t> (i)];
        if (slot.discrete.isDynamic() && (i == active || isCrossfading()))
            designDynamic (slot);
    }
}

void EqBand::designDynamic (Slot& slot) noexcept
{
    if (juce::exactlyEqual (slot.appliedChange[0], gainChange[0]) && juce::exactlyEqual (slot.appliedChange[1], gainChange[1]))
        return;

    auto s = settingsFor (slot);
    const auto staticGain = s.gainDb;

    s.gainDb = staticGain + gainChange[0];
    const auto first = BandDesign::design (s, sampleRate);

    if (juce::exactlyEqual (gainChange[0], gainChange[1]))
    {
        slot.processor.setCoefficients (first);
    }
    else
    {
        slot.processor.setChannelCoefficients (0, first);
        s.gainDb = staticGain + gainChange[1];
        slot.processor.setChannelCoefficients (1, BandDesign::design (s, sampleRate));
    }

    slot.appliedChange = gainChange;
}

const SectionCascade& EqBand::getActiveCascade() const noexcept
{
    return slots[static_cast<size_t> (active)].processor.getCascade();
}

BandSettings EqBand::getCurrentSettings() const noexcept
{
    return settingsFor (slots[static_cast<size_t> (active)]);
}

void EqBand::startCrossfade() noexcept
{
    auto& incoming = slots[static_cast<size_t> (1 - active)];
    incoming.discrete = target;
    incoming.processor.reset();
    incoming.processor.setCoefficients (BandDesign::design (settingsFor (incoming), sampleRate));
    incoming.appliedChange.fill (std::numeric_limits<double>::quiet_NaN());
    fadeRemaining = fadeLength;
}

void EqBand::updateCoefficients() noexcept
{
    auto& current = slots[static_cast<size_t> (active)];
    current.processor.setCoefficients (BandDesign::design (settingsFor (current), sampleRate));
    current.appliedChange.fill (std::numeric_limits<double>::quiet_NaN());

    if (isCrossfading())
    {
        auto& incoming = slots[static_cast<size_t> (1 - active)];
        incoming.processor.setCoefficients (BandDesign::design (settingsFor (incoming), sampleRate));
        incoming.appliedChange.fill (std::numeric_limits<double>::quiet_NaN());
    }
}

BandSettings EqBand::settingsFor (const Slot& slot) const noexcept
{
    auto s = slot.discrete;
    s.frequencyHz = frequency.getCurrentValue();
    s.gainDb = gainDb.getCurrentValue();
    s.q = q.getCurrentValue();
    return s;
}
