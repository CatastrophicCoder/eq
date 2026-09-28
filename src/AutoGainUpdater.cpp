#include "AutoGainUpdater.h"

#include "Parameters.h"
#include "dsp/AutoGain.h"

#include <algorithm>

AutoGainUpdater::AutoGainUpdater (juce::AudioProcessorValueTreeState& state, const std::array<std::atomic<bool>, 16>& bandInUse)
    : juce::Thread ("Auto Gain"), inUse (bandInUse)
{
    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        auto& p = bandParameters[static_cast<size_t> (band - 1)];
        p = { state.getRawParameterValue (Parameters::id (band, "freq")),
              state.getRawParameterValue (Parameters::id (band, "gain")),
              state.getRawParameterValue (Parameters::id (band, "q")),
              state.getRawParameterValue (Parameters::id (band, "type")),
              state.getRawParameterValue (Parameters::id (band, "slope")),
              state.getRawParameterValue (Parameters::id (band, "enabled")) };
        jassert (p.frequency != nullptr && p.gain != nullptr && p.q != nullptr
                 && p.type != nullptr && p.slope != nullptr && p.enabled != nullptr);
    }

    startThread (juce::Thread::Priority::low);
}

AutoGainUpdater::~AutoGainUpdater()
{
    stopThread (2000);
}

void AutoGainUpdater::setSampleRate (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
}

void AutoGainUpdater::run()
{
    std::array<BandSettings, 16> last {};
    double lastRate = 0.0;
    bool computed = false;

    while (! threadShouldExit())
    {
        const auto current = snapshot();
        const auto rate = sampleRate.load();

        const auto changed = ! computed || ! juce::exactlyEqual (rate, lastRate)
                          || ! std::equal (current.begin(), current.end(), last.begin(),
                                           [] (const auto& a, const auto& b) { return a.isIdenticalTo (b); });

        if (changed)
        {
            offsetDb = static_cast<float> (AutoGain::computeOffsetDb (current, rate));
            last = current;
            lastRate = rate;
            computed = true;
        }

        wait (pollIntervalMs);
    }
}

std::array<BandSettings, 16> AutoGainUpdater::snapshot() const
{
    std::array<BandSettings, 16> result;

    for (size_t i = 0; i < result.size(); ++i)
    {
        const auto& p = bandParameters[i];
        result[i] = Parameters::toBandSettings (p.type->load(), p.frequency->load(), p.gain->load(),
                                                p.q->load(), p.slope->load(), p.enabled->load(),
                                                inUse[i].load (std::memory_order_relaxed));
    }

    return result;
}
