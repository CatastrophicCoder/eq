#include "TestParameters.h"

#include "PluginEditor.h"
#include "dsp/CutSlope.h"
#include "ui/NodeDragController.h"
#include "ui/NodeLayout.h"
#include "ui/SelectionModel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <map>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace TestParameters;

namespace
{
    const juce::Rectangle<float> plot { 0.0f, 0.0f, 1000.0f, 400.0f };

    BandSettings make (FilterType type, double f, double gain, bool enabled = true)
    {
        BandSettings s;
        s.type = type; s.frequencyHz = f; s.gainDb = gain; s.q = 1.0; s.enabled = enabled;
        return s;
    }

    std::array<BandSettings, 16> disabled()
    {
        std::array<BandSettings, 16> b;
        for (auto& s : b)
            s.enabled = false;
        return b;
    }

    /** Counts gesture begin/end per parameter, so tests can require matched pairs. */
    struct GestureCounter final : juce::AudioProcessorParameter::Listener
    {
        explicit GestureCounter (juce::AudioProcessor& p)
        {
            for (auto* param : p.getParameters())
            {
                param->addListener (this);
                params.push_back (param);
            }
        }

        ~GestureCounter() override
        {
            for (auto* param : params)
                param->removeListener (this);
        }

        void parameterValueChanged (int, float) override {}
        void parameterGestureChanged (int index, bool starting) override { (starting ? begins : ends)[index]++; }

        int totalBegins() const { int n = 0; for (auto& [i, c] : begins) n += c; return n; }

        bool allMatched() const
        {
            for (auto& [i, c] : begins)
                if (ends.count (i) == 0 || ends.at (i) != c)
                    return false;
            for (auto& [i, c] : ends)
                if (begins.count (i) == 0)
                    return false;
            return true;
        }

        std::vector<juce::AudioProcessorParameter*> params;
        std::map<int, int> begins, ends;
    };

    struct DisplayFixture
    {
        juce::ScopedJuceInitialiser_GUI juce;
        ParametricEQAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> base { processor.createEditor() };
        ParametricEQAudioProcessorEditor& editor = *dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());
        ResponseDisplay& display = editor.getDisplay();

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

    const juce::ModifierKeys none {};
    const juce::ModifierKeys command { juce::ModifierKeys::commandModifier };
    const juce::ModifierKeys shift { juce::ModifierKeys::shiftModifier };
}

//==============================================================================
TEST_CASE ("Nodes sit at their frequency and gain, gainless types on 0 dB", "[interaction]")
{
    const FrequencyAxis axis (plot, 12.0);
    auto bands = disabled();
    bands[1] = make (FilterType::bell, 1000.0, 6.0);
    bands[4] = make (FilterType::lowCut, 80.0, 9.0);    // gain ignored: sits on 0 dB
    bands[9] = make (FilterType::highShelf, 8000.0, -20.0);   // beyond the range: held inside

    const auto nodes = NodeLayout::compute (bands, axis);
    REQUIRE (nodes.size() == 3);

    CHECK (nodes[0].band == 2);
    CHECK_THAT (nodes[0].position.x, WithinAbs (axis.xForFrequency (1000.0), 1e-3));
    CHECK_THAT (nodes[0].position.y, WithinAbs (axis.yForDb (6.0), 1e-3));
    CHECK (nodes[0].usesGain);

    CHECK (nodes[1].band == 5);
    CHECK_THAT (nodes[1].position.y, WithinAbs (axis.yForDb (0.0), 1e-3));
    CHECK_FALSE (nodes[1].usesGain);

    CHECK (nodes[2].band == 10);
    CHECK_THAT (nodes[2].position.y, WithinAbs (plot.getBottom(), 1e-3));
}

TEST_CASE ("Hit-testing picks the nearest node within the radius", "[interaction]")
{
    const std::vector<NodeLayout::Node> nodes { { 1, { 100.0f, 100.0f }, true }, { 2, { 112.0f, 100.0f }, true } };

    CHECK (NodeLayout::bandAt (nodes, { 103.0f, 100.0f }) == 1);
    CHECK (NodeLayout::bandAt (nodes, { 110.0f, 101.0f }) == 2);
    CHECK (NodeLayout::bandAt (nodes, { 100.0f, 115.0f }) == 0);   // 15 px away
    CHECK (NodeLayout::bandAt (nodes, { 400.0f, 300.0f }) == 0);

    CHECK (NodeLayout::bandsIn (nodes, { 90.0f, 90.0f, 15.0f, 20.0f }) == std::vector<int> { 1 });
    CHECK (NodeLayout::bandsIn (nodes, { 90.0f, 90.0f, 40.0f, 20.0f }) == std::vector<int> { 1, 2 });
}

TEST_CASE ("Selection model", "[interaction]")
{
    SelectionModel s;
    CHECK (s.isEmpty());
    CHECK (s.getPrimary() == 0);

    s.select (5);
    CHECK (s.getSelected() == std::vector<int> { 5 });
    CHECK (s.getPrimary() == 5);

    s.toggle (2);
    CHECK (s.getSelected() == std::vector<int> { 2, 5 });
    CHECK (s.getPrimary() == 2);

    s.toggle (2);
    CHECK (s.getSelected() == std::vector<int> { 5 });
    CHECK (s.getPrimary() == 5);

    s.add ({ 9, 1 });
    CHECK (s.getSelected() == std::vector<int> { 1, 5, 9 });

    s.remove (5);
    CHECK_FALSE (s.contains (5));
    CHECK (s.contains (1));
    CHECK ((s.getPrimary() == 1 || s.getPrimary() == 9));

    s.set ({ 3, 4 }, 4);
    CHECK (s.getSelected() == std::vector<int> { 3, 4 });
    CHECK (s.getPrimary() == 4);

    s.clear();
    CHECK (s.isEmpty());
    CHECK (s.getPrimary() == 0);
}

TEST_CASE ("Dragging moves frequency along the log axis and gain linearly", "[interaction]")
{
    const FrequencyAxis axis (plot, 12.0);
    NodeDragController drag;
    const auto start = juce::Point<float> { axis.xForFrequency (1000.0), axis.yForDb (0.0) };
    drag.begin ({ { 3, 1000.0, 0.0, true } }, start);
    CHECK (drag.isActive());

    // To the point for 2 kHz / +6 dB.
    const auto v = drag.dragTo ({ axis.xForFrequency (2000.0), axis.yForDb (6.0) }, false, axis);
    REQUIRE (v.size() == 1);
    CHECK (v[0].band == 3);
    CHECK_THAT (v[0].frequencyHz, WithinRel (2000.0, 1e-4));
    CHECK_THAT (v[0].gainDb, WithinAbs (6.0, 1e-3));

    // Fine mode: a quarter of the movement.
    const auto fine = drag.dragTo ({ axis.xForFrequency (2000.0), axis.yForDb (6.0) }, true, axis);
    CHECK_THAT (fine[0].frequencyHz, WithinRel (1000.0 * std::pow (2.0, 0.25), 1e-4));
    CHECK_THAT (fine[0].gainDb, WithinAbs (1.5, 1e-3));

    drag.end();
    CHECK_FALSE (drag.isActive());
}

TEST_CASE ("A multi-band drag keeps ratios and offsets, clamped per band", "[interaction]")
{
    const FrequencyAxis axis (plot, 12.0);
    NodeDragController drag;
    const auto start = juce::Point<float> { axis.xForFrequency (1000.0), axis.yForDb (0.0) };
    drag.begin ({ { 1, 1000.0, 0.0, true }, { 2, 250.0, -4.0, true }, { 3, 16000.0, 28.0, true }, { 4, 60.0, 0.0, false } },
                start);

    // One octave up, +3 dB.
    const auto v = drag.dragTo ({ axis.xForFrequency (2000.0), axis.yForDb (3.0) }, false, axis);
    REQUIRE (v.size() == 4);

    CHECK_THAT (v[0].frequencyHz, WithinRel (2000.0, 1e-4));
    CHECK_THAT (v[0].gainDb, WithinAbs (3.0, 1e-3));
    CHECK_THAT (v[1].frequencyHz, WithinRel (500.0, 1e-4));
    CHECK_THAT (v[1].gainDb, WithinAbs (-1.0, 1e-3));
    CHECK_THAT (v[2].frequencyHz, WithinAbs (20000.0, 1e-6));   // clamped at the top
    CHECK_THAT (v[2].gainDb, WithinAbs (30.0, 1e-6));           // clamped at +30
    CHECK_THAT (v[3].frequencyHz, WithinRel (120.0, 1e-4));
    CHECK_THAT (v[3].gainDb, WithinAbs (0.0, 0.0));             // no gain for this type
}

TEST_CASE ("Q scales per wheel step and stays in range", "[interaction]")
{
    CHECK_THAT (NodeDragController::wheelFactor (0.0f), WithinAbs (1.0, 0.0));
    CHECK (NodeDragController::wheelFactor (0.1f) > 1.0);
    CHECK (NodeDragController::wheelFactor (-0.1f) < 1.0);
    CHECK_THAT (NodeDragController::wheelFactor (0.1f) * NodeDragController::wheelFactor (-0.1f), WithinAbs (1.0, 1e-12));

    CHECK_THAT (NodeDragController::scaleQ (1.0, 2.0), WithinAbs (2.0, 1e-12));
    CHECK_THAT (NodeDragController::scaleQ (10.0, 5.0), WithinAbs (18.0, 0.0));
    CHECK_THAT (NodeDragController::scaleQ (0.2, 0.1), WithinAbs (0.1, 0.0));
}

//==============================================================================
TEST_CASE ("Clicking selects a node; Cmd-click toggles; the panel follows", "[interaction][display]")
{
    DisplayFixture f;
    setBand (f.processor, 3, FilterType::bell, 200.0f, 4.0f, 1.0f, 3, true);
    setBand (f.processor, 7, FilterType::bell, 2000.0f, -3.0f, 1.0f, 3, true);

    f.display.handlePress (f.nodeOf (3), none, 1);
    f.display.handleRelease();
    CHECK (f.display.getSelection().getSelected() == std::vector<int> { 3 });
    CHECK (f.editor.getBandPanel().isVisible());
    CHECK (f.editor.getBandPanel().getBand() == 3);

    f.display.handlePress (f.nodeOf (7), command, 1);
    f.display.handleRelease();
    CHECK (f.display.getSelection().getSelected() == std::vector<int> { 3, 7 });
    CHECK (f.editor.getBandPanel().getBand() == 7);

    f.display.handlePress (f.nodeOf (3), command, 1);
    f.display.handleRelease();
    CHECK (f.display.getSelection().getSelected() == std::vector<int> { 7 });

    // A plain click on empty space clears the selection and hides the panel.
    f.display.handlePress (f.at (50.0, -9.0), none, 1);
    f.display.handleRelease();
    CHECK (f.display.getSelection().isEmpty());
    CHECK_FALSE (f.editor.getBandPanel().isVisible());
}

TEST_CASE ("Dragging a node writes frequency and gain inside one gesture", "[interaction][display]")
{
    DisplayFixture f;
    setBand (f.processor, 5, FilterType::bell, 1000.0f, 0.0f, 1.0f, 3, true);
    GestureCounter gestures (f.processor);

    const auto start = f.nodeOf (5);
    f.display.handlePress (start, none, 1);
    f.display.handleDrag (f.at (1500.0, 2.0), none);
    f.display.handleDrag (f.at (2000.0, 4.5), none);
    f.display.handleRelease();

    CHECK_THAT (value (f.processor, "band5_freq"), WithinRel (2000.0f, 2e-3f));
    CHECK_THAT (value (f.processor, "band5_gain"), WithinAbs (4.5f, 0.02f));
    CHECK (gestures.totalBegins() == 2);   // freq and gain, once each for the whole drag
    CHECK (gestures.allMatched());
}

TEST_CASE ("Dragging one of several selected nodes moves them all", "[interaction][display]")
{
    DisplayFixture f;
    setBand (f.processor, 2, FilterType::bell, 500.0f, 0.0f, 1.0f, 3, true);
    setBand (f.processor, 9, FilterType::bell, 4000.0f, -2.0f, 1.0f, 3, true);
    f.display.setSelection ({ 2, 9 }, 2);
    GestureCounter gestures (f.processor);

    f.display.handlePress (f.nodeOf (2), none, 1);    // already selected: keep the selection
    f.display.handleDrag (f.at (1000.0, 3.0), none);  // one octave up, +3 dB
    f.display.handleRelease();

    CHECK_THAT (value (f.processor, "band2_freq"), WithinRel (1000.0f, 2e-3f));
    CHECK_THAT (value (f.processor, "band2_gain"), WithinAbs (3.0f, 0.02f));
    CHECK_THAT (value (f.processor, "band9_freq"), WithinRel (8000.0f, 2e-3f));
    CHECK_THAT (value (f.processor, "band9_gain"), WithinAbs (1.0f, 0.02f));
    CHECK (gestures.allMatched());
}

TEST_CASE ("Dragging empty space selects the nodes inside the rectangle", "[interaction][display]")
{
    DisplayFixture f;
    setBand (f.processor, 1, FilterType::bell, 100.0f, 3.0f, 1.0f, 3, true);
    setBand (f.processor, 2, FilterType::bell, 300.0f, -3.0f, 1.0f, 3, true);
    setBand (f.processor, 3, FilterType::bell, 5000.0f, 0.0f, 1.0f, 3, true);
    f.editor.refreshControls();

    f.display.handlePress (f.at (60.0, 9.0), none, 1);
    f.display.handleDrag (f.at (600.0, -9.0), none);
    CHECK (f.display.isSelectingArea());
    f.display.handleRelease();
    CHECK_FALSE (f.display.isSelectingArea());
    CHECK (f.display.getSelection().getSelected() == std::vector<int> { 1, 2 });

    // Shift adds to the existing selection.
    f.display.handlePress (f.at (3000.0, 6.0), shift, 1);
    f.display.handleDrag (f.at (8000.0, -6.0), shift);
    f.display.handleRelease();
    CHECK (f.display.getSelection().getSelected() == std::vector<int> { 1, 2, 3 });
}

TEST_CASE ("Wheel and pinch change Q of the node under the pointer", "[interaction][display]")
{
    DisplayFixture f;
    setBand (f.processor, 4, FilterType::bell, 800.0f, 3.0f, 1.0f, 3, true);
    GestureCounter gestures (f.processor);

    f.display.handleWheel (f.nodeOf (4), 0.25f);
    CHECK_THAT (value (f.processor, "band4_q"), WithinRel (static_cast<float> (NodeDragController::wheelFactor (0.25f)), 1e-3f));

    f.display.handleMagnify (f.nodeOf (4), 0.5f);
    CHECK_THAT (value (f.processor, "band4_q"), WithinRel (static_cast<float> (0.5 * NodeDragController::wheelFactor (0.25f)), 1e-3f));

    // Away from any node and with nothing selected: nothing changes.
    const auto before = value (f.processor, "band4_q");
    f.display.handleWheel (f.at (30.0, -10.0), 0.25f);
    CHECK_THAT (value (f.processor, "band4_q"), WithinAbs (before, 0.0f));

    CHECK (gestures.allMatched());
}

TEST_CASE ("Double-click on empty space adds a Bell in the first free band", "[interaction][display]")
{
    DisplayFixture f;
    setBand (f.processor, 1, FilterType::lowCut, 30.0f, 0.0f, 0.71f, 3, true);
    GestureCounter gestures (f.processor);

    f.display.handlePress (f.at (1200.0, 5.0), none, 2);
    f.display.handleRelease();

    CHECK_THAT (value (f.processor, "band2_enabled"), WithinAbs (1.0f, 0.0f));
    CHECK_THAT (value (f.processor, "band2_type"), WithinAbs (static_cast<float> (FilterType::bell), 0.0f));
    CHECK_THAT (value (f.processor, "band2_freq"), WithinRel (1200.0f, 2e-3f));
    CHECK_THAT (value (f.processor, "band2_gain"), WithinAbs (5.0f, 0.02f));
    CHECK (f.display.getSelection().getSelected() == std::vector<int> { 2 });
    CHECK (gestures.allMatched());
}

TEST_CASE ("With all 16 bands in use a double-click adds nothing and says so", "[interaction][display]")
{
    DisplayFixture f;
    for (int band = 1; band <= 16; ++band)
        setBand (f.processor, band, FilterType::bell, 50.0f * static_cast<float> (band), 0.0f, 1.0f, 3, true);
    f.editor.refreshControls();

    // Somewhere no node sits.
    f.display.handlePress (f.at (15000.0, 10.0), none, 2);
    f.display.handleRelease();

    for (int band = 1; band <= 16; ++band)
        CHECK_THAT (value (f.processor, Parameters::id (band, "freq")), WithinRel (50.0f * static_cast<float> (band), 1e-3f));
    CHECK (f.display.getMessage().contains ("16"));
}

TEST_CASE ("Double-click on a node disables that band", "[interaction][display]")
{
    DisplayFixture f;
    setBand (f.processor, 6, FilterType::bell, 900.0f, 6.0f, 1.0f, 3, true);

    f.display.handlePress (f.nodeOf (6), none, 1);
    f.display.handleRelease();
    f.display.handlePress (f.nodeOf (6), none, 2);
    f.display.handleRelease();

    CHECK_THAT (value (f.processor, "band6_enabled"), WithinAbs (0.0f, 0.0f));
    CHECK_THAT (value (f.processor, "band6_gain"), WithinAbs (6.0f, 0.01f));   // settings kept
    CHECK_FALSE (f.display.getSelection().contains (6));
}

TEST_CASE ("The node menu offers types, slopes for cuts, and disable", "[interaction][display]")
{
    DisplayFixture f;
    setBand (f.processor, 2, FilterType::bell, 400.0f, 3.0f, 1.0f, 3, true);
    setBand (f.processor, 3, FilterType::highCut, 9000.0f, 0.0f, 0.71f, 5, true);

    auto countItems = [] (const juce::PopupMenu& menu, int from, int to)
    {
        int n = 0;
        for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
            if (const auto id = it.getItem().itemID; id >= from && id < to)
                ++n;
        return n;
    };

    auto isTicked = [] (const juce::PopupMenu& menu, int id)
    {
        for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
            if (it.getItem().itemID == id)
                return it.getItem().isTicked;
        return false;
    };

    const auto bellMenu = f.display.buildNodeMenu (2);
    CHECK (countItems (bellMenu, ResponseDisplay::menuTypeBase, ResponseDisplay::menuTypeBase + FilterTypes::count) == FilterTypes::count);
    CHECK (countItems (bellMenu, ResponseDisplay::menuSlopeBase, ResponseDisplay::menuSlopeBase + CutSlope::count) == 0);
    CHECK (countItems (bellMenu, ResponseDisplay::menuDisable, ResponseDisplay::menuDisable + 1) == 1);
    CHECK (isTicked (bellMenu, ResponseDisplay::menuTypeBase + static_cast<int> (FilterType::bell)));

    const auto cutMenu = f.display.buildNodeMenu (3);
    CHECK (countItems (cutMenu, ResponseDisplay::menuSlopeBase, ResponseDisplay::menuSlopeBase + CutSlope::count) == CutSlope::count);
    CHECK (isTicked (cutMenu, ResponseDisplay::menuSlopeBase + 5));
}

TEST_CASE ("Node menu choices apply to the selection they belong to", "[interaction][display]")
{
    DisplayFixture f;
    setBand (f.processor, 2, FilterType::bell, 400.0f, 3.0f, 1.0f, 3, true);
    setBand (f.processor, 3, FilterType::bell, 900.0f, 3.0f, 1.0f, 3, true);
    setBand (f.processor, 4, FilterType::bell, 3000.0f, 3.0f, 1.0f, 3, true);
    f.display.setSelection ({ 2, 3 }, 2);
    GestureCounter gestures (f.processor);

    // On a selected node: both selected bands change, band 4 does not.
    f.display.applyNodeMenuResult (3, ResponseDisplay::menuTypeBase + static_cast<int> (FilterType::notch));
    CHECK_THAT (value (f.processor, "band2_type"), WithinAbs (static_cast<float> (FilterType::notch), 0.0f));
    CHECK_THAT (value (f.processor, "band3_type"), WithinAbs (static_cast<float> (FilterType::notch), 0.0f));
    CHECK_THAT (value (f.processor, "band4_type"), WithinAbs (static_cast<float> (FilterType::bell), 0.0f));

    // On a node outside the selection: only that band.
    f.display.applyNodeMenuResult (4, ResponseDisplay::menuTypeBase + static_cast<int> (FilterType::highCut));
    f.display.applyNodeMenuResult (4, ResponseDisplay::menuSlopeBase + CutSlope::brickwallIndex);
    CHECK_THAT (value (f.processor, "band4_type"), WithinAbs (static_cast<float> (FilterType::highCut), 0.0f));
    CHECK_THAT (value (f.processor, "band4_slope"), WithinAbs (static_cast<float> (CutSlope::brickwallIndex), 0.0f));
    CHECK_THAT (value (f.processor, "band2_type"), WithinAbs (static_cast<float> (FilterType::notch), 0.0f));

    f.display.applyNodeMenuResult (2, ResponseDisplay::menuDisable);
    CHECK_THAT (value (f.processor, "band2_enabled"), WithinAbs (0.0f, 0.0f));
    CHECK_THAT (value (f.processor, "band3_enabled"), WithinAbs (0.0f, 0.0f));
    CHECK (f.display.getSelection().isEmpty());

    CHECK (gestures.allMatched());
}

TEST_CASE ("Delete disables the selected bands and leaves other keys to the host", "[interaction][display]")
{
    DisplayFixture f;
    setBand (f.processor, 5, FilterType::bell, 600.0f, 2.0f, 1.0f, 3, true);
    setBand (f.processor, 8, FilterType::bell, 6000.0f, 2.0f, 1.0f, 3, true);
    f.display.setSelection ({ 5, 8 }, 5);

    CHECK_FALSE (f.display.handleKey (juce::KeyPress (juce::KeyPress::spaceKey)));   // e.g. transport
    CHECK (f.display.handleKey (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK_THAT (value (f.processor, "band5_enabled"), WithinAbs (0.0f, 0.0f));
    CHECK_THAT (value (f.processor, "band8_enabled"), WithinAbs (0.0f, 0.0f));
    CHECK (f.display.getSelection().isEmpty());

    setBand (f.processor, 5, FilterType::bell, 600.0f, 2.0f, 1.0f, 3, true);
    f.display.setSelection ({ 5 }, 5);
    CHECK (f.display.handleKey (juce::KeyPress (juce::KeyPress::backspaceKey)));
    CHECK_THAT (value (f.processor, "band5_enabled"), WithinAbs (0.0f, 0.0f));
}

TEST_CASE ("A band disabled elsewhere drops out of the selection", "[interaction][display]")
{
    DisplayFixture f;
    setBand (f.processor, 5, FilterType::bell, 600.0f, 2.0f, 1.0f, 3, true);
    f.editor.refreshControls();
    f.display.setSelection ({ 5 }, 5);
    CHECK (f.editor.getBandPanel().isVisible());

    set (f.processor, "band5_enabled", 0.0f);   // e.g. from the panel, automation or the host
    f.editor.refreshControls();
    CHECK (f.display.getSelection().isEmpty());
    CHECK_FALSE (f.editor.getBandPanel().isVisible());
}
