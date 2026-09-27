#include "dsp/CascadeProcessor.h"

#include <juce_dsp/juce_dsp.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    /** A random stable section: poles and zeros inside the unit circle. */
    BiquadCoefficients randomSection (juce::Random& random, bool firstOrder)
    {
        const auto pick = [&] (double lo, double hi) { return lo + (hi - lo) * random.nextDouble(); };

        BiquadCoefficients c;

        if (firstOrder)
        {
            c.a1 = -pick (0.0, 0.95);
            c.b0 = pick (0.1, 1.0);
            c.b1 = pick (-0.9, 0.9);
            return c;
        }

        const auto poleRadius = pick (0.1, 0.95), poleAngle = pick (0.01, 3.1);
        const auto zeroRadius = pick (0.0, 1.0),  zeroAngle = pick (0.0, 3.14);
        const auto gain = pick (0.1, 2.0);

        c.a1 = -2.0 * poleRadius * std::cos (poleAngle);
        c.a2 = poleRadius * poleRadius;
        c.b0 = gain;
        c.b1 = -2.0 * gain * zeroRadius * std::cos (zeroAngle);
        c.b2 = gain * zeroRadius * zeroRadius;
        return c;
    }
}

TEST_CASE ("CascadeProcessor matches a chain of juce::dsp::IIR::Filter", "[cascade]")
{
    juce::Random random (42);

    for (int trial = 0; trial < 20; ++trial)
    {
        SectionCascade cascade;
        const auto numSections = 1 + random.nextInt (SectionCascade::maxSections);

        for (int i = 0; i < numSections; ++i)
            cascade.add (randomSection (random, i == numSections - 1 && random.nextBool()));

        CascadeProcessor processor;
        processor.setCoefficients (cascade);
        processor.reset();

        std::vector<juce::dsp::IIR::Filter<double>> reference;
        for (int i = 0; i < numSections; ++i)
        {
            const auto& s = cascade.sections[static_cast<size_t> (i)];
            reference.emplace_back (new juce::dsp::IIR::Coefficients<double> (s.b0, s.b1, s.b2, 1.0, s.a1, s.a2));
            reference.back().reset();
        }

        for (int n = 0; n < 2000; ++n)
        {
            const auto input = random.nextDouble() * 2.0 - 1.0;

            auto expected = input;
            for (auto& filter : reference)
                expected = filter.processSample (expected);

            INFO ("trial=" << trial << " sections=" << numSections << " n=" << n);
            REQUIRE_THAT (processor.processSample (0, input), WithinAbs (expected, 1e-12));
        }
    }
}

TEST_CASE ("CascadeProcessor keeps channels independent", "[cascade]")
{
    SectionCascade cascade;
    cascade.add ({ 0.5, 0.3, 0.1, -0.4, 0.2 });

    CascadeProcessor processor;
    processor.setCoefficients (cascade);
    processor.reset();

    // Drive only channel 0; channel 1 must still answer its first impulse like a fresh filter.
    for (int n = 0; n < 10; ++n)
        processor.processSample (0, 1.0);

    CHECK_THAT (processor.processSample (1, 1.0), WithinAbs (0.5, 1e-15));
}

TEST_CASE ("CascadeProcessor reset clears the state", "[cascade]")
{
    SectionCascade cascade;
    cascade.add ({ 1.0, 0.5, 0.25, -0.9, 0.3 });

    CascadeProcessor processor;
    processor.setCoefficients (cascade);

    for (int n = 0; n < 100; ++n)
        processor.processSample (0, 1.0);

    processor.reset();
    CHECK_THAT (processor.processSample (0, 0.0), WithinAbs (0.0, 0.0));
}

TEST_CASE ("CascadeProcessor with no sections passes audio through unchanged", "[cascade]")
{
    CascadeProcessor processor;
    processor.setCoefficients ({});

    for (auto x : { 0.0, 1.0, -0.5, 0.123456789 })
        CHECK (juce::exactlyEqual (processor.processSample (0, x), x));
}

TEST_CASE ("CascadeProcessor starts newly used sections from zero state", "[cascade]")
{
    // Section 2 has b = 0, so its output is only its own state s1.
    const BiquadCoefficients first { 1.0, 0.0, 0.0, -0.5, 0.0 };
    const BiquadCoefficients stateOnly { 0.0, 1.0, 0.0, 0.0, 0.0 };

    SectionCascade one;
    one.add (first);

    SectionCascade two = one;
    two.add (stateOnly);

    CascadeProcessor processor;
    processor.setCoefficients (two);
    processor.reset();

    // Leave non-zero state in section 2, shrink to one section, then grow back.
    for (int n = 0; n < 50; ++n)
        processor.processSample (0, 1.0);

    processor.setCoefficients (one);
    processor.setCoefficients (two);

    // Section 2 starts from zero state: with b0 = 0 its first output is exactly 0.
    CHECK_THAT (processor.processSample (0, 1.0), WithinAbs (0.0, 0.0));
}
