#include "TestParameters.h"

#include "PluginEditor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using namespace TestParameters;

namespace
{
    void prepare (ParametricEQAudioProcessor& p)
    {
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
    }

    juce::AudioBuffer<float> noise (int numSamples, int seed)
    {
        juce::AudioBuffer<float> b (2, numSamples);
        juce::Random random (seed);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < numSamples; ++i)
                b.setSample (ch, i, random.nextFloat() - 0.5f);
        return b;
    }
}

TEST_CASE ("Nothing is pushed to the analyzer while no editor is open", "[analyzertap]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    prepare (p);
    CHECK_FALSE (p.isAnalyzerActive());

    auto buffer = noise (1024, 1);
    juce::MidiBuffer midi;
    p.processBlock (buffer, midi);

    CHECK (p.getPreFifo().getNumReady() == 0);
    CHECK (p.getPostFifo().getNumReady() == 0);
}

TEST_CASE ("The taps carry the input (pre) and the output (post)", "[analyzertap]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    set (p, Parameters::outputInvert, 1.0f);   // output = -input, exactly
    prepare (p);
    p.setAnalyzerActive (true);

    CHECK (p.getPreFifo().getNumChannels() == 2);
    CHECK (p.getPostFifo().getNumChannels() == 2);
    CHECK (p.getPreFifo().getCapacity() == ParametricEQAudioProcessor::analyzerFifoCapacity);

    auto buffer = noise (1024, 2);
    juce::AudioBuffer<float> input;
    input.makeCopyOf (buffer);
    juce::MidiBuffer midi;
    p.processBlock (buffer, midi);

    juce::AudioBuffer<float> pre (2, 1024), post (2, 1024);
    REQUIRE (p.getPreFifo().pull (pre) == 1024);
    REQUIRE (p.getPostFifo().pull (post) == 1024);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 1024; ++i)
        {
            REQUIRE (juce::exactlyEqual (pre.getSample (ch, i), input.getSample (ch, i)));
            REQUIRE (juce::exactlyEqual (post.getSample (ch, i), -input.getSample (ch, i)));
        }
}

TEST_CASE ("Opening the editor switches the taps on, closing switches them off", "[analyzertap]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    prepare (p);

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        CHECK (p.isAnalyzerActive());
    }

    CHECK_FALSE (p.isAnalyzerActive());
}

TEST_CASE ("Analyzer settings are saved with the session; invalid values fall back", "[analyzertap][state]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    const AnalyzerSettings::Values defaults;
    CHECK (ParametricEQAudioProcessor().getAnalyzerSettings().mode == defaults.mode);

    juce::MemoryBlock saved;
    {
        ParametricEQAudioProcessor source;
        source.setAnalyzerSettings ({ static_cast<int> (AnalyzerSettings::Mode::post), 3, 2, 2 });
        source.getStateInformation (saved);
    }

    ParametricEQAudioProcessor target;
    target.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    const auto v = target.getAnalyzerSettings();
    CHECK (v.mode == static_cast<int> (AnalyzerSettings::Mode::post));
    CHECK (v.resolution == 3);
    CHECK (v.speed == 2);
    CHECK (v.range == 2);

    target.setAnalyzerSettings ({ 9, -1, 7, 42 });   // all out of range
    const auto fallback = target.getAnalyzerSettings();
    CHECK (fallback.mode == defaults.mode);
    CHECK (fallback.resolution == defaults.resolution);
    CHECK (fallback.speed == defaults.speed);
    CHECK (fallback.range == defaults.range);
}

TEST_CASE ("The display analyses the taps and the meter follows the output", "[analyzertap][editor]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    prepare (p);
    std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
    auto& editor = *dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());

    // A 0.5 sine through the plugin, then one editor refresh.
    juce::AudioBuffer<float> buffer (2, 8192);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 8192; ++i)
            buffer.setSample (ch, i, static_cast<float> (0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / 48000.0)));
    juce::MidiBuffer midi;
    for (int start = 0; start < 8192; start += 512)
    {
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, start, 512);
        p.processBlock (view, midi);
    }
    editor.refreshControls();

    auto& display = editor.getDisplay();
    double loudestPre = SpectrumAnalyzer::floorDb, loudestPost = SpectrumAnalyzer::floorDb;
    for (int k = 0; k < SpectrumAnalyzer::numPoints; ++k)
    {
        loudestPre = std::max (loudestPre, display.getPreAnalyzer().levelDb (k));
        loudestPost = std::max (loudestPost, display.getPostAnalyzer().levelDb (k));
    }
    CHECK_THAT (loudestPre, WithinAbs (-6.0, 1.0));
    CHECK_THAT (loudestPost, WithinAbs (-6.0, 1.0));
    CHECK_THAT (display.getMeter().peakDb (0), WithinAbs (-6.02, 0.1));

    // Mode "Off": no analysis, but the meter keeps running.
    p.setAnalyzerSettings ({ static_cast<int> (AnalyzerSettings::Mode::off), 1, 1, 1 });
    display.getPreAnalyzer().reset();
    display.getPostAnalyzer().reset();
    for (int start = 0; start < 8192; start += 512)
    {
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, start, 512);
        p.processBlock (view, midi);
    }
    editor.refreshControls();
    CHECK_THAT (display.getPostAnalyzer().levelDb (100), WithinAbs (SpectrumAnalyzer::floorDb, 0.0));
    CHECK (display.getMeter().peakDb (0) > -7.0);
}

TEST_CASE ("Bottom bar analyzer controls change the stored settings", "[analyzertap][editor]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
    auto& editor = *dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());
    auto& bar = editor.getBottomBar();

    bar.getAnalyzerModeBox().setSelectedItemIndex (static_cast<int> (AnalyzerSettings::Mode::pre), juce::sendNotificationSync);
    bar.getResolutionBox().setSelectedItemIndex (3, juce::sendNotificationSync);
    bar.getSpeedBox().setSelectedItemIndex (0, juce::sendNotificationSync);
    bar.getRangeBox().setSelectedItemIndex (2, juce::sendNotificationSync);

    const auto v = p.getAnalyzerSettings();
    CHECK (v.mode == static_cast<int> (AnalyzerSettings::Mode::pre));
    CHECK (v.resolution == 3);
    CHECK (v.speed == 0);
    CHECK (v.range == 2);
    CHECK (editor.getDisplay().getPreAnalyzer().getFftSize() == 16384);

    bar.getFreezeButton().setToggleState (true, juce::sendNotificationSync);
    CHECK (editor.getDisplay().getPreAnalyzer().isFrozen());
    CHECK (editor.getDisplay().getPostAnalyzer().isFrozen());

    // Every analyzer menu shows distinct text, so they can be told apart at a glance.
    bar.showAnalyzerSettings ({});
    const juce::StringArray shown { bar.getAnalyzerModeBox().getText(), bar.getResolutionBox().getText(),
                                    bar.getSpeedBox().getText(), bar.getRangeBox().getText() };
    for (int i = 0; i < shown.size(); ++i)
        for (int j = i + 1; j < shown.size(); ++j)
            CHECK (shown[i] != shown[j]);
    CHECK (bar.getResolutionBox().getTooltip().isNotEmpty());
    CHECK (bar.getSpeedBox().getTooltip().isNotEmpty());

    // Bottom bar controls do not overlap, at the smallest window size too.
    editor.setSize (ParametricEQAudioProcessorEditor::minWidth, ParametricEQAudioProcessorEditor::minHeight);
    std::vector<juce::Rectangle<int>> boxes;
    for (auto* child : bar.getChildren())
        if (child->isVisible())
            boxes.push_back (child->getBounds());
    for (size_t i = 0; i < boxes.size(); ++i)
        for (size_t j = i + 1; j < boxes.size(); ++j)
        {
            INFO (boxes[i].toString() << " vs " << boxes[j].toString());
            CHECK_FALSE (boxes[i].intersects (boxes[j]));
        }
}
