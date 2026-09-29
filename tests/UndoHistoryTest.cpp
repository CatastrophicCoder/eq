#include "TestParameters.h"

#include "PluginEditor.h"
#include "presets/UndoHistory.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <map>

using Catch::Matchers::WithinAbs;
using namespace TestParameters;

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

    struct Fixture
    {
        juce::ScopedJuceInitialiser_GUI juce;
        ParametricEQAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> base { processor.createEditor() };
        ParametricEQAudioProcessorEditor& editor = *dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());
        ResponseDisplay& display = editor.getDisplay();
        UndoHistory& undo = processor.getUndoHistory();

        Fixture()
        {
            // Starting setting, made without recording (as a loaded session would be).
            {
                UndoHistory::ScopedSuspend suspend (undo);
                setBand (processor, 3, FilterType::bell, 500.0f, 4.0f, 1.0f, 3, true);
                setBand (processor, 8, FilterType::highShelf, 6000.0f, -2.0f, 0.71f, 3, true);
            }
            undo.clear();
            editor.refreshControls();
        }

        /** A complete user edit of one parameter, as a knob or menu makes it. */
        void edit (const juce::String& id, float value)
        {
            auto& p = param (processor, id);
            p.beginChangeGesture();
            p.setValueNotifyingHost (p.convertTo0to1 (value));
            p.endChangeGesture();
        }

        juce::Point<float> nodeOf (int band)
        {
            editor.refreshControls();
            for (const auto& n : display.getNodes())
                if (n.band == band)
                    return n.position;
            FAIL ("no node for band " << band);
            return {};
        }

        juce::Point<float> at (double frequencyHz, double gainDb) const
        {
            const auto axis = display.getAxis();
            return { axis.xForFrequency (frequencyHz), axis.yForDb (gainDb) };
        }
    };
}

//==============================================================================
TEST_CASE ("A knob or menu edit is one undo step", "[undo]")
{
    Fixture f;
    CHECK_FALSE (f.undo.canUndo());

    f.edit ("band3_gain", -6.0f);
    REQUIRE (f.undo.getNumSteps() == 1);

    CHECK (f.undo.undo());
    CHECK_THAT (value (f.processor, "band3_gain"), WithinAbs (4.0f, 1e-4f));
    CHECK (f.undo.canRedo());
    CHECK (f.undo.redo());
    CHECK_THAT (value (f.processor, "band3_gain"), WithinAbs (-6.0f, 1e-4f));
    CHECK_FALSE (f.undo.canRedo());
}

TEST_CASE ("A drag of several selected nodes is one step", "[undo]")
{
    Fixture f;
    f.display.setSelection ({ 3, 8 }, 3);
    const auto start = f.nodeOf (3);
    f.display.handlePress (start, {}, 1);
    for (int i = 1; i <= 10; ++i)
        f.display.handleDrag (start + juce::Point<float> (6.0f * static_cast<float> (i), -3.0f * static_cast<float> (i)), {});
    f.display.handleRelease();

    REQUIRE (f.undo.getNumSteps() == 1);
    const auto moved3 = value (f.processor, "band3_freq"), moved8 = value (f.processor, "band8_freq");
    CHECK (moved3 > 500.0f);
    CHECK (moved8 > 6000.0f);

    f.undo.undo();
    CHECK_THAT (value (f.processor, "band3_freq"), WithinAbs (500.0f, 0.01f));
    CHECK_THAT (value (f.processor, "band8_freq"), WithinAbs (6000.0f, 0.1f));
    CHECK_THAT (value (f.processor, "band3_gain"), WithinAbs (4.0f, 1e-3f));
}

TEST_CASE ("Adding, deleting and enabling bands are undoable steps", "[undo]")
{
    Fixture f;

    SECTION ("add by double-click")
    {
        f.display.handlePress (f.at (2000.0, 5.0), {}, 2);
        REQUIRE (f.undo.getNumSteps() == 1);
        CHECK (f.processor.isBandInUse (1));
        f.undo.undo();
        CHECK_FALSE (f.processor.isBandInUse (1));
        f.undo.redo();
        CHECK (f.processor.isBandInUse (1));
        CHECK_THAT (value (f.processor, "band1_freq"), WithinAbs (2000.0f, 5.0f));
    }

    SECTION ("delete with the key")
    {
        f.display.setSelection ({ 3, 8 }, 3);
        REQUIRE (f.display.handleKey (juce::KeyPress (juce::KeyPress::deleteKey)));
        REQUIRE (f.undo.getNumSteps() == 1);
        CHECK_FALSE (f.processor.isBandInUse (3));
        f.undo.undo();
        CHECK (f.processor.isBandInUse (3));
        CHECK (f.processor.isBandInUse (8));
        CHECK_THAT (value (f.processor, "band3_enabled"), WithinAbs (1.0f, 0.0f));
    }

    SECTION ("disable from the node menu")
    {
        f.display.applyNodeMenuResult (8, ResponseDisplay::menuToggleEnable);
        REQUIRE (f.undo.getNumSteps() == 1);
        CHECK_THAT (value (f.processor, "band8_enabled"), WithinAbs (0.0f, 0.0f));
        f.undo.undo();
        CHECK_THAT (value (f.processor, "band8_enabled"), WithinAbs (1.0f, 0.0f));
    }

    SECTION ("type change for a selection from the node menu")
    {
        f.display.setSelection ({ 3, 8 }, 3);
        f.display.applyNodeMenuResult (3, ResponseDisplay::menuTypeBase + static_cast<int> (FilterType::notch));
        REQUIRE (f.undo.getNumSteps() == 1);
        f.undo.undo();
        CHECK (f.processor.getBandSettings()[2].type == FilterType::bell);
        CHECK (f.processor.getBandSettings()[7].type == FilterType::highShelf);
    }
}

TEST_CASE ("Phase mode and length changes are undoable, latency included", "[undo][linearphase]")
{
    Fixture f;
    auto& controls = f.editor.getTopBar().getPhaseModeControls();
    controls.getModeBox().setSelectedItemIndex (1, juce::sendNotificationSync);
    controls.getLengthBox().setSelectedItemIndex (2, juce::sendNotificationSync);
    REQUIRE (f.undo.getNumSteps() == 2);
    CHECK (f.processor.getLatencySamples() > 0);

    f.undo.undo();
    CHECK (f.processor.getLinearPhaseLength() == 0);
    f.undo.undo();
    CHECK_FALSE (f.processor.isLinearPhase());
    CHECK (f.processor.getLatencySamples() == 0);
}

TEST_CASE ("Wheel steps on Q in quick succession merge into one step", "[undo]")
{
    Fixture f;
    for (int i = 0; i < 12; ++i)
        f.display.handleWheel (f.nodeOf (3), 0.25f);
    CHECK (f.undo.getNumSteps() == 1);
    f.undo.undo();
    CHECK_THAT (value (f.processor, "band3_q"), WithinAbs (1.0f, 1e-3f));
}

TEST_CASE ("Host automation, view settings and non-edits are not recorded", "[undo]")
{
    Fixture f;

    // Automation: values without gestures.
    auto& gain = param (f.processor, "band3_gain");
    for (int i = 0; i < 10; ++i)
        gain.setValueNotifyingHost (static_cast<float> (i) / 10.0f);
    CHECK (f.undo.getNumSteps() == 0);

    // View settings.
    f.processor.setDisplayRangeDb (30.0);
    auto analyzer = f.processor.getAnalyzerSettings();
    analyzer.speed = 0;
    f.processor.setAnalyzerSettings (analyzer);
    CHECK (f.undo.getNumSteps() == 0);

    // A gesture that ends where it started changes nothing.
    auto& q = param (f.processor, "band3_q");
    q.beginChangeGesture();
    q.endChangeGesture();
    CHECK (f.undo.getNumSteps() == 0);
}

TEST_CASE ("Preset loads, A/B switches and session loads clear the history", "[undo]")
{
    Fixture f;
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("SpectralFaultTests-" + juce::Uuid().toString());
    folder.createDirectory();
    f.processor.getPresetManager().setUserFolder (folder);

    f.edit ("band3_gain", -6.0f);
    REQUIRE (f.undo.canUndo());
    const auto entries = f.processor.getPresetManager().getEntries();
    REQUIRE (f.processor.getPresetManager().load (entries.front()));
    CHECK_FALSE (f.undo.canUndo());   // the load itself is not a step either

    f.edit ("band3_gain", -3.0f);
    REQUIRE (f.undo.canUndo());
    f.processor.getAbComparison().switchTo (AbComparison::Slot::b);
    CHECK_FALSE (f.undo.canUndo());

    f.edit ("band3_gain", -2.0f);
    REQUIRE (f.undo.canUndo());
    juce::MemoryBlock state;
    f.processor.getStateInformation (state);
    f.processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    CHECK_FALSE (f.undo.canUndo());
    CHECK_FALSE (f.undo.canRedo());

    folder.deleteRecursively();
}

TEST_CASE ("A new edit after undo clears redo; the history keeps the last 100 steps", "[undo]")
{
    Fixture f;
    f.edit ("band3_gain", -1.0f);
    f.edit ("band3_gain", -2.0f);
    f.undo.undo();
    REQUIRE (f.undo.canRedo());
    f.edit ("band3_q", 3.0f);
    CHECK_FALSE (f.undo.canRedo());

    f.undo.clear();
    for (int i = 1; i <= 120; ++i)
        f.edit ("band3_gain", static_cast<float> (i % 20) - 10.0f + 0.01f * static_cast<float> (i));
    CHECK (f.undo.getNumSteps() == UndoHistory::maxSteps);

    while (f.undo.canUndo())
        f.undo.undo();
    // The oldest kept step is edit 21: undoing it gives the value after edit 20.
    CHECK_THAT (value (f.processor, "band3_gain"), WithinAbs (static_cast<float> (20 % 20) - 10.0f + 0.2f, 1e-3f));
}

TEST_CASE ("Undo sends complete host edits for changed parameters only, and records nothing itself", "[undo]")
{
    Fixture f;
    f.edit ("band3_gain", -6.0f);
    f.edit ("band8_freq", 4000.0f);
    REQUIRE (f.undo.getNumSteps() == 2);

    GestureCounter gestures (f.processor);
    f.undo.undo();
    CHECK (gestures.allMatched());
    CHECK (gestures.begins.size() == 1);   // band8_freq only
    CHECK (f.undo.getNumSteps() == 1);
    CHECK (f.undo.canRedo());
}

TEST_CASE ("Undo and redo buttons in the top bar", "[undo][editor]")
{
    Fixture f;
    auto& bar = f.editor.getTopBar();
    f.editor.refreshControls();
    CHECK_FALSE (bar.getUndoButton().isEnabled());
    CHECK_FALSE (bar.getRedoButton().isEnabled());

    f.edit ("band3_gain", -6.0f);
    f.editor.refreshControls();
    CHECK (bar.getUndoButton().isEnabled());

    REQUIRE (bar.getUndoButton().onClick != nullptr);
    bar.getUndoButton().onClick();
    CHECK_THAT (value (f.processor, "band3_gain"), WithinAbs (4.0f, 1e-4f));
    CHECK_FALSE (bar.getUndoButton().isEnabled());
    CHECK (bar.getRedoButton().isEnabled());
    bar.getRedoButton().onClick();
    CHECK_THAT (value (f.processor, "band3_gain"), WithinAbs (-6.0f, 1e-4f));

    using E = ParametricEQAudioProcessorEditor;
    for (auto [w, h] : { std::pair { E::minWidth, E::minHeight }, { E::defaultWidth, E::defaultHeight }, { E::maxWidth, E::maxHeight } })
    {
        f.editor.setSize (w, h);
        INFO ("size " << w << "x" << h);
        const auto nameRight = 14.0f + juce::GlyphArrangement::getStringWidth (
            juce::FontOptions (static_cast<float> (bar.getHeight()) * 0.5f, juce::Font::bold), JucePlugin_Name);
        for (auto* b : { &bar.getUndoButton(), &bar.getRedoButton() })
        {
            CHECK (bar.getLocalBounds().contains (b->getBounds()));
            CHECK (b->getRight() < bar.getAButton().getX());
            CHECK (static_cast<float> (b->getX()) > nameRight);
            CHECK (b->getWidth() >= 24);
        }
    }
}
