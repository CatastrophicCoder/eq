#include "PeakingBand.h"

#include "MatchedPeakingDesign.h"

#include <algorithm>
#include <array>

PeakingBand::PeakingBand()
    : sharedCoefficients (new juce::dsp::IIR::Coefficients<double> (1.0, 0.0, 0.0, 1.0, 0.0, 0.0))
{
}

void PeakingBand::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;

    filters.clear();
    for (int ch = 0; ch < numChannels; ++ch)
        filters.emplace_back (sharedCoefficients);

    frequency.reset (sampleRate, rampSeconds);
    gainDb.reset (sampleRate, rampSeconds);
    q.reset (sampleRate, rampSeconds);

    frequency.setCurrentAndTargetValue (targetFrequency);
    gainDb.setCurrentAndTargetValue (targetGainDb);
    q.setCurrentAndTargetValue (targetQ);

    updateCoefficients (targetFrequency, targetGainDb, targetQ);

    // Filter::reset() allocates its state the first time it sees a second-order
    // filter; doing it here keeps that off the audio thread.
    for (auto& filter : filters)
        filter.reset();
}

void PeakingBand::setTargets (double frequencyHz, double newGainDb, double newQ) noexcept
{
    targetFrequency = frequencyHz;
    targetGainDb = newGainDb;
    targetQ = newQ;

    frequency.setTargetValue (targetFrequency);
    gainDb.setTargetValue (targetGainDb);
    q.setTargetValue (targetQ);
}

void PeakingBand::reset() noexcept
{
    for (auto& filter : filters)
        filter.reset();
}

void PeakingBand::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto numChannels = std::min (buffer.getNumChannels(), static_cast<int> (filters.size()));
    const auto numSamples = buffer.getNumSamples();

    for (int start = 0; start < numSamples; start += subBlockSize)
    {
        const auto length = std::min (subBlockSize, numSamples - start);

        if (frequency.isSmoothing() || gainDb.isSmoothing() || q.isSmoothing())
            updateCoefficients (frequency.skip (length), gainDb.skip (length), q.skip (length));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* x = buffer.getWritePointer (ch, start);
            auto& filter = filters[static_cast<size_t> (ch)];

            for (int i = 0; i < length; ++i)
                x[i] = static_cast<float> (filter.processSample (static_cast<double> (x[i])));
        }
    }

    for (auto& filter : filters)
        filter.snapToZero();
}

void PeakingBand::updateCoefficients (double frequencyHz, double newGainDb, double newQ) noexcept
{
    // Keep the centre strictly below Nyquist, whatever the host sample rate.
    const auto safeFrequency = std::min (frequencyHz, 0.49 * sampleRate);
    current = MatchedPeakingDesign::design (safeFrequency, newGainDb, newQ, sampleRate);

    // Assigning in place reuses the storage reserved at construction: no allocation.
    *sharedCoefficients = std::array<double, 6> { current.b0, current.b1, current.b2, 1.0, current.a1, current.a2 };
}
