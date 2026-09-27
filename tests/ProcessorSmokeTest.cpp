#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>

TEST_CASE ("Processor reports a stereo in / stereo out layout", "[processor]")
{
    ParametricEQAudioProcessor processor;

    CHECK (processor.getTotalNumInputChannels() == 2);
    CHECK (processor.getTotalNumOutputChannels() == 2);
    CHECK_FALSE (processor.acceptsMidi());
    CHECK_FALSE (processor.producesMidi());
}

TEST_CASE ("Processor passes audio through unchanged", "[processor]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    constexpr int blockSize = 512;
    constexpr int numChannels = 2;

    ParametricEQAudioProcessor processor;
    processor.setPlayConfigDetails (numChannels, numChannels, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < blockSize; ++i)
            buffer.setSample (ch, i, static_cast<float> (0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / sampleRate)));

    juce::AudioBuffer<float> expected;
    expected.makeCopyOf (buffer);

    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const auto sample = buffer.getSample (ch, i);
            REQUIRE (std::isfinite (sample));
            REQUIRE (juce::exactlyEqual (sample, expected.getSample (ch, i)));
        }
    }

    processor.releaseResources();
}
