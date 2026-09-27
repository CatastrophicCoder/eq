#include "CascadeProcessor.h"

void CascadeProcessor::setCoefficients (const SectionCascade& newCascade) noexcept
{
    // Sections beyond the old count start from silence rather than stale state.
    for (auto& channel : state)
        for (int i = cascade.numSections; i < newCascade.numSections; ++i)
            channel[static_cast<size_t> (i)] = {};

    cascade = newCascade;
}

void CascadeProcessor::reset() noexcept
{
    for (auto& channel : state)
        channel.fill ({});
}

double CascadeProcessor::processSample (int channel, double input) noexcept
{
    auto& sections = state[static_cast<size_t> (channel)];
    auto x = input;

    for (int i = 0; i < cascade.numSections; ++i)
    {
        const auto& c = cascade.sections[static_cast<size_t> (i)];
        auto& s = sections[static_cast<size_t> (i)];

        // Transposed direct form II.
        const auto y = c.b0 * x + s.s1;
        s.s1 = c.b1 * x - c.a1 * y + s.s2;
        s.s2 = c.b2 * x - c.a2 * y;
        x = y;
    }

    return x;
}
