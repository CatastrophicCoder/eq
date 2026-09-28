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

void EqBand::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto channels = std::min (buffer.getNumChannels(), numChannels);
    const auto numSamples = buffer.getNumSamples();

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

        auto& current = slots[static_cast<size_t> (active)].processor;

        if (! isCrossfading())
        {
            // A bypassed band (no sections) leaves the audio untouched: skip the loop.
            if (current.getCascade().numSections == 0)
                continue;

            for (int ch = 0; ch < channels; ++ch)
            {
                auto* x = buffer.getWritePointer (ch, start);

                for (int i = 0; i < length; ++i)
                    x[i] = static_cast<float> (current.processSample (ch, static_cast<double> (x[i])));
            }

            continue;
        }

        // Linear crossfade from the current to the incoming filter.
        auto& incoming = slots[static_cast<size_t> (1 - active)].processor;
        const auto done = fadeLength - fadeRemaining;

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* x = buffer.getWritePointer (ch, start);

            for (int i = 0; i < length; ++i)
            {
                const auto in = static_cast<double> (x[i]);
                const auto g = std::min (1.0, static_cast<double> (done + i + 1) / fadeLength);
                x[i] = static_cast<float> ((1.0 - g) * current.processSample (ch, in) + g * incoming.processSample (ch, in));
            }
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
