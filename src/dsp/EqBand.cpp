#include "EqBand.h"

#include "BandDesign.h"

#include <algorithm>
#include <cmath>

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

void EqBand::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto channels = std::min (buffer.getNumChannels(), numChannels);
    const auto twoChannels = channels >= 2;
    const auto numSamples = buffer.getNumSamples();
    auto* left = buffer.getWritePointer (0);
    auto* right = twoChannels ? buffer.getWritePointer (1) : nullptr;

    for (int start = 0; start < numSamples; start += subBlockSize)
    {
        const auto length = std::min (subBlockSize, numSamples - start);

        if (! isCrossfading() && ! target.hasSameDiscreteSettings (slots[static_cast<size_t> (active)].discrete))
            startCrossfade();

        if (frequency.isSmoothing() || gainDb.isSmoothing() || q.isSmoothing())
        {
            frequency.skip (length);
            gainDb.skip (length);
            q.skip (length);
            updateCoefficients();
        }

        auto& current = slots[static_cast<size_t> (active)];

        if (! isCrossfading())
        {
            // A bypassed band (no sections) leaves the audio untouched: skip the loop.
            if (current.processor.getCascade().numSections == 0)
                continue;

            for (int i = start; i < start + length; ++i)
            {
                auto l = static_cast<double> (left[i]);
                auto r = twoChannels ? static_cast<double> (right[i]) : 0.0;
                route (current.processor, current.discrete.channel, twoChannels, l, r);
                left[i] = static_cast<float> (l);
                if (twoChannels)
                    right[i] = static_cast<float> (r);
            }

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
    }
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
    fadeRemaining = fadeLength;
}

void EqBand::updateCoefficients() noexcept
{
    auto& current = slots[static_cast<size_t> (active)];
    current.processor.setCoefficients (BandDesign::design (settingsFor (current), sampleRate));

    if (isCrossfading())
    {
        auto& incoming = slots[static_cast<size_t> (1 - active)];
        incoming.processor.setCoefficients (BandDesign::design (settingsFor (incoming), sampleRate));
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
