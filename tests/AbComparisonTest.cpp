#include "TestParameters.h"

#include "PluginEditor.h"
#include "presets/AbComparison.h"
#include "presets/FactoryPresets.h"
#include "ui/FrequencyAxis.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <map>

using Catch::Matchers::WithinAbs;
using namespace TestParameters;
using Slot = AbComparison::Slot;

namespace
{
    struct GestureCounter final : juce::AudioProcessorParameter::Listener
    {
        explicit GestureCounter (juce::AudioProcessor& p)
        {
            for (auto* param : p.getParameters()) { param->addListener (this); params.push_back (param); }
        }
        ~GestureCounter() override { for (auto* param : params) param->removeListener (this); }
        void parameterValueChanged (int, float) override {}
        void parameterGestureChanged (int index, bool starting) override { (starting ? begins : ends)[index]++; }
        bool allMatched() const
        {
            for (auto& [i, c] : begins) if (ends.count (i) == 0 || ends.at (i) != c) return false;
            return begins.size() == ends.size();
        }
        std::vector<juce::AudioProcessorParameter*> params;
        std::map<int, int> begins, ends;
    };

    /** Setting "A": a few bands, dynamics, output, Zero latency, default view. */
    void settingA (ParametricEQAudioProcessor& p)
    {
        setBand (p, 2, FilterType::bell, 900.0f, 4.0f, 1.2f, 3, true);
        setBand (p, 7, FilterType::highShelf, 7000.0f, -3.0f, 0.71f, 3, false);
        set (p, "band2_dyn", 1.0f);
        set (p, "band2_thresh", -33.0f);
        set (p, Parameters::outputGain, -2.0f);
    }
}

//==============================================================================
TEST_CASE ("A/B starts on A with both slots equal to the current setting", "[ab]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    settingA (p);

    auto& ab = p.getAbComparison();
    CHECK (ab.getActive() == Slot::a);

    ab.switchTo (Slot::b);   // B has never been set: it equals what A was when first captured
    CHECK (ab.getActive() == Slot::b);
    CHECK_THAT (value (p, "band2_gain"), WithinAbs (4.0f, 1e-4f));
    CHECK (p.isBandInUse (2));
}

TEST_CASE ("Each slot keeps its own edits across switches", "[ab]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    settingA (p);
    auto& ab = p.getAbComparison();

    ab.switchTo (Slot::b);
    // B: different bands, a freed band, phase mode, view settings.
    set (p, "band2_gain", -6.0f);
    set (p, "band2_dyn", 0.0f);
    p.setBandInUse (7, false);
    setBand (p, 11, FilterType::notch, 3000.0f, 0.0f, 6.0f, 3, true);
    set (p, Parameters::outputInvert, 1.0f);
    p.setLinearPhase (true);
    p.setLinearPhaseLength (2);
    p.setDisplayRangeDb (30.0);
    auto analyzer = p.getAnalyzerSettings();
    analyzer.speed = 0;
    p.setAnalyzerSettings (analyzer);

    ab.switchTo (Slot::a);
    CHECK_THAT (value (p, "band2_gain"), WithinAbs (4.0f, 1e-4f));
    CHECK_THAT (value (p, "band2_dyn"), WithinAbs (1.0f, 0.0f));
    CHECK_THAT (value (p, "band2_thresh"), WithinAbs (-33.0f, 1e-4f));
    CHECK (p.isBandInUse (7));
    CHECK_FALSE (p.isBandInUse (11));
    CHECK_THAT (value (p, Parameters::outputInvert), WithinAbs (0.0f, 0.0f));
    CHECK_THAT (value (p, Parameters::outputGain), WithinAbs (-2.0f, 1e-4f));
    CHECK_FALSE (p.isLinearPhase());
    CHECK (p.getLatencySamples() == 0);
    CHECK_THAT (p.getDisplayRangeDb(), WithinAbs (FrequencyAxis::defaultRangeDb, 0.0));
    CHECK (p.getAnalyzerSettings().speed != 0);

    ab.switchTo (Slot::b);
    CHECK_THAT (value (p, "band2_gain"), WithinAbs (-6.0f, 1e-4f));
    CHECK_THAT (value (p, "band2_dyn"), WithinAbs (0.0f, 0.0f));
    CHECK_FALSE (p.isBandInUse (7));
    CHECK (p.isBandInUse (11));
    CHECK (p.getBandSettings()[10].type == FilterType::notch);
    CHECK_THAT (value (p, Parameters::outputInvert), WithinAbs (1.0f, 0.0f));
    CHECK (p.isLinearPhase());
    CHECK (p.getLinearPhaseLength() == 2);
    CHECK (p.getLatencySamples() > 0);
    CHECK_THAT (p.getDisplayRangeDb(), WithinAbs (30.0, 0.0));
    CHECK (p.getAnalyzerSettings().speed == 0);
}

TEST_CASE ("Copy puts the active setting into the other slot", "[ab]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    settingA (p);
    auto& ab = p.getAbComparison();

    ab.switchTo (Slot::b);
    set (p, "band2_gain", -9.0f);
    ab.switchTo (Slot::a);
    CHECK_THAT (value (p, "band2_gain"), WithinAbs (4.0f, 1e-4f));

    set (p, "band2_gain", 1.5f);
    ab.copyActiveToOther();
    CHECK (ab.getActive() == Slot::a);
    ab.switchTo (Slot::b);
    CHECK_THAT (value (p, "band2_gain"), WithinAbs (1.5f, 1e-4f));
}

TEST_CASE ("Switching sends host edits only for parameters that differ", "[ab]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    settingA (p);
    auto& ab = p.getAbComparison();
    ab.switchTo (Slot::b);
    set (p, "band2_gain", -6.0f);
    set (p, "band7_freq", 5000.0f);

    GestureCounter gestures (p);
    ab.switchTo (Slot::a);

    CHECK (gestures.allMatched());
    CHECK (gestures.begins.size() == 2);   // band2_gain and band7_freq, nothing else
}

TEST_CASE ("Both slots and the active one are saved with the session", "[ab][state]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    juce::MemoryBlock saved;
    {
        ParametricEQAudioProcessor source;
        settingA (source);
        auto& ab = source.getAbComparison();
        ab.switchTo (Slot::b);
        set (source, "band2_gain", -6.0f);
        source.setLinearPhase (true);
        source.getStateInformation (saved);   // active: B
    }

    ParametricEQAudioProcessor target;
    target.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    auto& ab = target.getAbComparison();
    CHECK (ab.getActive() == Slot::b);
    CHECK_THAT (value (target, "band2_gain"), WithinAbs (-6.0f, 1e-4f));
    CHECK (target.isLinearPhase());

    ab.switchTo (Slot::a);
    CHECK_THAT (value (target, "band2_gain"), WithinAbs (4.0f, 1e-4f));
    CHECK_FALSE (target.isLinearPhase());

    // The A/B data is not part of the setting itself: saving again keeps exactly one copy of it.
    juce::MemoryBlock again;
    target.getStateInformation (again);
    const auto xml = juce::AudioProcessor::getXmlFromBinary (again.getData(), static_cast<int> (again.getSize()));
    REQUIRE (xml != nullptr);
    CHECK (xml->getIntAttribute ("stateVersion") == 5);
    int abElements = 0;
    for (auto* child : xml->getChildIterator())
        if (child->hasTagName (AbComparison::stateTag))
            ++abElements;
    CHECK (abElements == 1);
}

TEST_CASE ("Sessions from before A/B load with both slots equal", "[ab][state]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    juce::MemoryBlock saved;
    {
        ParametricEQAudioProcessor source;
        settingA (source);
        source.getStateInformation (saved);
    }

    // Remove the A/B element, as a version 4 state would be.
    auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), static_cast<int> (saved.getSize()));
    REQUIRE (xml != nullptr);
    xml->deleteAllChildElementsWithTagName (AbComparison::stateTag);
    xml->setAttribute ("stateVersion", 4);
    juce::MemoryBlock v4;
    juce::AudioProcessor::copyXmlToBinary (*xml, v4);

    ParametricEQAudioProcessor target;
    target.setStateInformation (v4.getData(), static_cast<int> (v4.getSize()));
    auto& ab = target.getAbComparison();
    CHECK (ab.getActive() == Slot::a);
    ab.switchTo (Slot::b);
    CHECK_THAT (value (target, "band2_gain"), WithinAbs (4.0f, 1e-4f));
    CHECK_THAT (value (target, "band2_thresh"), WithinAbs (-33.0f, 1e-4f));
}

TEST_CASE ("Loading a preset changes only the active slot", "[ab][preset]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("SpectralFaultTests-" + juce::Uuid().toString());
    folder.createDirectory();
    p.getPresetManager().setUserFolder (folder);

    settingA (p);
    auto& ab = p.getAbComparison();
    ab.switchTo (Slot::b);

    const auto entries = p.getPresetManager().getEntries();
    const auto kick = std::find_if (entries.begin(), entries.end(), [] (const auto& e) { return e.name == "Kick"; });
    REQUIRE (kick != entries.end());
    REQUIRE (p.getPresetManager().load (*kick));
    CHECK (p.getStoredPresetName() == "Kick");

    ab.switchTo (Slot::a);
    CHECK_THAT (value (p, "band2_gain"), WithinAbs (4.0f, 1e-4f));
    CHECK (p.getStoredPresetName().isEmpty());

    ab.switchTo (Slot::b);
    CHECK (p.getStoredPresetName() == "Kick");
    CHECK_FALSE (p.getPresetManager().isModified());

    folder.deleteRecursively();
}

TEST_CASE ("A/B buttons in the top bar switch, copy and show the active slot", "[ab][editor]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor p;
    settingA (p);
    std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
    auto& editor = *dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());
    auto& bar = editor.getTopBar();

    CHECK (bar.getAButton().getToggleState());
    CHECK_FALSE (bar.getBButton().getToggleState());
    CHECK (bar.getCopyButton().getButtonText() == juce::String::fromUTF8 ("A\xe2\x86\x92" "B"));

    REQUIRE (bar.getBButton().onClick != nullptr);
    bar.getBButton().onClick();   // what a click calls (triggerClick is asynchronous)
    CHECK (p.getAbComparison().getActive() == Slot::b);
    CHECK (bar.getBButton().getToggleState());
    CHECK_FALSE (bar.getAButton().getToggleState());
    CHECK (bar.getCopyButton().getButtonText() == juce::String::fromUTF8 ("B\xe2\x86\x92" "A"));

    set (p, "band2_gain", -3.0f);
    REQUIRE (bar.getCopyButton().onClick != nullptr);
    bar.getCopyButton().onClick();   // what a click calls (triggerClick is asynchronous)
    REQUIRE (bar.getAButton().onClick != nullptr);
    bar.getAButton().onClick();   // what a click calls (triggerClick is asynchronous)
    CHECK_THAT (value (p, "band2_gain"), WithinAbs (-3.0f, 1e-4f));

    // Layout: between the plugin name and the preset browser at every size.
    using E = ParametricEQAudioProcessorEditor;
    for (auto [w, h] : { std::pair { E::minWidth, E::minHeight }, { E::defaultWidth, E::defaultHeight }, { E::maxWidth, E::maxHeight } })
    {
        editor.setSize (w, h);
        INFO ("size " << w << "x" << h);
        const auto nameRight = 14.0f + juce::GlyphArrangement::getStringWidth (
            juce::FontOptions (static_cast<float> (bar.getHeight()) * 0.5f, juce::Font::bold), JucePlugin_Name);
        for (auto* b : { &bar.getAButton(), &bar.getBButton(), &bar.getCopyButton() })
        {
            CHECK (bar.getLocalBounds().contains (b->getBounds()));
            CHECK (b->getRight() < bar.getPreviousButton().getX());
            CHECK (static_cast<float> (b->getX()) > nameRight);   // clear of the plugin name
            CHECK (b->getWidth() >= 24);
        }
    }
}
