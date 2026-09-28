#include "CascadeProcessor.h"

void CascadeProcessor::setCoefficients (const SectionCascade& newCascade) noexcept
{
    for (int channel = 0; channel < maxChannels; ++channel)
        setChannelCoefficients (channel, newCascade);
}

void CascadeProcessor::setChannelCoefficients (int channel, const SectionCascade& newCascade) noexcept
{
    auto& cascade = cascades[static_cast<size_t> (channel)];
    auto& sections = state[static_cast<size_t> (channel)];

    // Sections beyond the old count start from silence rather than stale state.
    for (int i = cascade.numSections; i < newCascade.numSections; ++i)
        sections[static_cast<size_t> (i)] = {};

    // Copy only the sections in use: dynamic bands reload coefficients every few samples.
    for (int i = 0; i < newCascade.numSections; ++i)
        cascade.sections[static_cast<size_t> (i)] = newCascade.sections[static_cast<size_t> (i)];
    cascade.numSections = newCascade.numSections;
}

void CascadeProcessor::reset() noexcept
{
    for (auto& channel : state)
        channel.fill ({});
}

double CascadeProcessor::processSample (int channel, double input) noexcept
{
    const auto& cascade = cascades[static_cast<size_t> (channel)];
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
