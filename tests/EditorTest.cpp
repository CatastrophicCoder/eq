#include "TestParameters.h"

#include "PluginEditor.h"
#include "dsp/BandDesign.h"
#include "dsp/CutSlope.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace TestParameters;

namespace
{
    struct EditorFixture
    {
        juce::ScopedJuceInitialiser_GUI juce;
        ParametricEQAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> base { processor.createEditor() };
        ParametricEQAudioProcessorEditor& editor = *dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());
    };

    using E = ParametricEQAudioProcessorEditor;
    const std::pair<int, int> sizes[] { { E::minWidth, E::minHeight }, { E::defaultWidth, E::defaultHeight },
                                         { E::maxWidth, E::maxHeight } };

    /** Bounds of c in the editor's coordinate space. */
    juce::Rectangle<int> inEditor (juce::Component& c, juce::Component& editor)
    {
        return editor.getLocalArea (c.getParentComponent(), c.getBounds());
    }

    /** Every visible descendant sits inside the editor and has a usable size. */
    void checkLaidOut (juce::Component& c, juce::Component& editor, const juce::String& path)
    {
        for (auto* child : c.getChildren())
        {
            if (! child->isVisible())
                continue;

            const auto bounds = inEditor (*child, editor);
            INFO (path << "/" << child->getName() << " " << bounds.toString());
            CHECK_FALSE (child->getBounds().isEmpty());
            CHECK (editor.getLocalBounds().contains (bounds));
            checkLaidOut (*child, editor, path + "/" + child->getName());
        }
    }

    /** The box shows the selected item's text, and the text fits its label. */
    bool menuReadable (juce::ComboBox& box, int expectedIndex)
    {
        INFO ("shown '" << box.getText() << "', item " << expectedIndex << " is '" << box.getItemText (expectedIndex) << "'");
        CHECK (box.getText().isNotEmpty());
        CHECK (box.getText() == box.getItemText (expectedIndex));

        auto* label = dynamic_cast<juce::Label*> (box.getChildComponent (0));
        REQUIRE (label != nullptr);
        const auto textArea = label->getBorderSize().subtractedFrom (label->getLocalBounds());
        const auto width = juce::GlyphArrangement::getStringWidth (label->getFont(), box.getText());
        INFO ("needs " << width << " px, has " << textArea.getWidth());
        return width <= static_cast<float> (textArea.getWidth());
    }
}

//==============================================================================
TEST_CASE ("Editor has a top bar, display, band panel and bottom bar at the planned size", "[editor]")
{
    EditorFixture f;

    CHECK (f.editor.getWidth() == E::defaultWidth);
    CHECK (f.editor.getHeight() == E::defaultHeight);
    CHECK (f.editor.isResizable());

    auto* constrainer = f.editor.getConstrainer();
    REQUIRE (constrainer != nullptr);
    CHECK (constrainer->getMinimumWidth() == E::minWidth);
    CHECK (constrainer->getMinimumHeight() == E::minHeight);
    CHECK (constrainer->getMaximumWidth() == E::maxWidth);
    CHECK (constrainer->getMaximumHeight() == E::maxHeight);

    CHECK (f.editor.getTopBar().isVisible());
    CHECK (f.editor.getDisplay().isVisible());
    CHECK (f.editor.getBottomBar().isVisible());

    // With nothing selected the band panel is hidden; it opens on the selected band.
    CHECK_FALSE (f.editor.getBandPanel().isVisible());
    setBand (f.processor, 6, FilterType::bell, 700.0f, 3.0f, 1.0f, 3, true);
    f.editor.refreshControls();
    f.editor.getDisplay().setSelection ({ 6 }, 6);
    CHECK (f.editor.getBandPanel().isVisible());
    CHECK (f.editor.getBandPanel().getBand() == 6);
    f.editor.getDisplay().setSelection ({}, 0);
    CHECK_FALSE (f.editor.getBandPanel().isVisible());
}

TEST_CASE ("Layout follows the planned arrangement at every size", "[editor]")
{
    EditorFixture f;
    setBand (f.processor, 4, FilterType::bell, 500.0f, 3.0f, 1.0f, 3, true);
    f.editor.refreshControls();
    f.editor.getDisplay().setSelection ({ 4 }, 4);   // the panel shows only with a selection

    for (auto [w, h] : sizes)
    {
        f.editor.setSize (w, h);
        INFO ("size " << w << "x" << h);

        const auto top = f.editor.getTopBar().getBounds();
        const auto display = f.editor.getDisplay().getBounds();
        const auto bottom = f.editor.getBottomBar().getBounds();
        const auto panel = inEditor (f.editor.getBandPanel(), f.editor);

        // Thin full-width bars at the top and bottom, the display filling the space between.
        CHECK (top.getY() == 0);
        CHECK (top.getWidth() == w);
        CHECK (top.getHeight() <= h / 12);
        CHECK (bottom.getBottom() == h);
        CHECK (bottom.getWidth() == w);
        CHECK (bottom.getHeight() <= h / 12);
        CHECK (display.getY() == top.getBottom());
        CHECK (display.getBottom() == bottom.getY());
        CHECK (display.getWidth() == w);

        // The band panel sits over the lower part of the display, centred, above the frequency labels.
        CHECK (display.contains (panel));
        CHECK (std::abs (panel.getCentreX() - display.getCentreX()) <= 2);
        CHECK (panel.getY() > display.getCentreY());
        CHECK (panel.getBottom() <= display.getBottom() - ResponseDisplay::labelStripHeight);

        checkLaidOut (f.editor, f.editor, "editor");

        auto& bp = f.editor.getBandPanel();
        for (auto* knob : { &bp.getFrequencySlider(), &bp.getGainSlider(), &bp.getQSlider() })
        {
            CHECK (knob->getWidth() >= 30);
            CHECK (knob->getHeight() >= 30);
        }
    }
}

TEST_CASE ("The band panel's controls follow the band it shows", "[editor]")
{
    EditorFixture f;
    auto& panel = f.editor.getBandPanel();
    auto& p = f.processor;

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        INFO ("band " << band);
        panel.setBand (band);
        CHECK (panel.getBand() == band);

        // Control -> parameter.
        panel.getFrequencySlider().setValue (2345.0, juce::sendNotificationSync);
        panel.getGainSlider().setValue (-7.25, juce::sendNotificationSync);
        panel.getQSlider().setValue (3.3, juce::sendNotificationSync);
        panel.getTypeBox().setSelectedItemIndex (static_cast<int> (FilterType::notch), juce::sendNotificationSync);
        panel.getSlopeBox().setSelectedItemIndex (CutSlope::brickwallIndex, juce::sendNotificationSync);
        panel.getEnableButton().setToggleState (true, juce::sendNotificationSync);

        CHECK_THAT (value (p, Parameters::id (band, "freq")), WithinRel (2345.0f, 1e-3f));
        CHECK_THAT (value (p, Parameters::id (band, "gain")), WithinAbs (-7.25f, 0.01f));
        CHECK_THAT (value (p, Parameters::id (band, "q")), WithinRel (3.3f, 1e-3f));
        CHECK_THAT (value (p, Parameters::id (band, "type")), WithinAbs (static_cast<float> (FilterType::notch), 0.0f));
        CHECK_THAT (value (p, Parameters::id (band, "slope")), WithinAbs (static_cast<float> (CutSlope::brickwallIndex), 0.0f));
        CHECK_THAT (value (p, Parameters::id (band, "enabled")), WithinAbs (1.0f, 0.0f));

        // Parameter -> control.
        setBand (p, band, FilterType::highShelf, 432.0f, 11.5f, 0.9f, 5, false);
        CHECK_THAT (panel.getFrequencySlider().getValue(), WithinRel (432.0, 1e-3));
        CHECK_THAT (panel.getGainSlider().getValue(), WithinAbs (11.5, 0.01));
        CHECK_THAT (panel.getQSlider().getValue(), WithinRel (0.9, 1e-3));
        CHECK (panel.getTypeBox().getSelectedItemIndex() == static_cast<int> (FilterType::highShelf));
        CHECK (panel.getSlopeBox().getSelectedItemIndex() == 5);
        CHECK_FALSE (panel.getEnableButton().getToggleState());
    }

    // Switching bands must not write to the band left behind.
    panel.setBand (3);
    const auto before = value (p, Parameters::id (2, "freq"));
    panel.getFrequencySlider().setValue (777.0, juce::sendNotificationSync);
    CHECK_THAT (value (p, Parameters::id (2, "freq")), WithinAbs (before, 0.0f));
    CHECK_THAT (value (p, Parameters::id (3, "freq")), WithinRel (777.0f, 1e-3f));
}

TEST_CASE ("Menus list the parameter choices and stay readable at every size", "[editor]")
{
    EditorFixture f;
    auto& panel = f.editor.getBandPanel();
    auto& p = f.processor;

    REQUIRE (panel.getTypeBox().getNumItems() == FilterTypes::count);
    REQUIRE (panel.getSlopeBox().getNumItems() == CutSlope::count);

    for (auto [w, h] : sizes)
    {
        f.editor.setSize (w, h);
        INFO ("size " << w << "x" << h);

        for (int t = 0; t < FilterTypes::count; ++t)
        {
            set (p, Parameters::id (1, "type"), static_cast<float> (t));
            CHECK (menuReadable (panel.getTypeBox(), t));
        }

        for (int s = 0; s < CutSlope::count; ++s)
        {
            set (p, Parameters::id (1, "slope"), static_cast<float> (s));
            CHECK (menuReadable (panel.getSlopeBox(), s));
        }
    }
}

TEST_CASE ("Controls a type does not use are greyed out", "[editor]")
{
    EditorFixture f;
    auto& panel = f.editor.getBandPanel();
    setBand (f.processor, 3, FilterType::bell, 500.0f, 0.0f, 1.0f, 3, true);   // enabled: disabled bands grey everything
    panel.setBand (3);

    for (int t = 0; t < FilterTypes::count; ++t)
    {
        const auto type = static_cast<FilterType> (t);
        set (f.processor, Parameters::id (3, "type"), static_cast<float> (t));
        f.editor.refreshControls();

        INFO ("type " << FilterTypes::names[t]);
        CHECK (panel.getGainSlider().isEnabled() == FilterTypes::usesGain (type));
        CHECK (panel.getQSlider().isEnabled() == FilterTypes::usesQ (type));
        CHECK (panel.getSlopeBox().isEnabled() == FilterTypes::usesSlope (type));
        CHECK (panel.getFrequencySlider().isEnabled());
    }
}

TEST_CASE ("Bottom bar controls are attached and show the Auto Gain correction", "[editor]")
{
    EditorFixture f;
    auto& p = f.processor;
    auto& bar = f.editor.getBottomBar();

    bar.getGainSlider().setValue (-4.5, juce::sendNotificationSync);
    bar.getAutoGainButton().setToggleState (true, juce::sendNotificationSync);
    bar.getInvertButton().setToggleState (true, juce::sendNotificationSync);
    CHECK_THAT (value (p, Parameters::outputGain), WithinAbs (-4.5f, 1e-4f));
    CHECK_THAT (value (p, Parameters::autoGain), WithinAbs (1.0f, 0.0f));
    CHECK_THAT (value (p, Parameters::outputInvert), WithinAbs (1.0f, 0.0f));

    set (p, Parameters::outputGain, 7.0f);
    set (p, Parameters::outputInvert, 0.0f);
    CHECK_THAT (bar.getGainSlider().getValue(), WithinAbs (7.0, 1e-4));
    CHECK_FALSE (bar.getInvertButton().getToggleState());

    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    setBand (p, 5, FilterType::bell, 1000.0f, 9.0f, 1.0f, 3, true);
    for (int i = 0; i < 200 && std::abs (p.getAutoGainOffsetDb()) < 0.5f; ++i)
        juce::Thread::sleep (10);
    REQUIRE (std::abs (p.getAutoGainOffsetDb()) > 0.5f);

    f.editor.refreshControls();
    CHECK (bar.getOffsetLabel().getText().contains (juce::String (p.getAutoGainOffsetDb(), 1) + " dB"));
}

TEST_CASE ("The range button cycles 3, 6, 12, 30 dB and stores the choice", "[editor]")
{
    EditorFixture f;
    auto& display = f.editor.getDisplay();
    auto& button = display.getRangeButton();

    CHECK (button.getButtonText() == "12 dB");
    CHECK_THAT (display.getAxis().getRangeDb(), WithinAbs (12.0, 0.0));

    for (auto expected : { 30.0, 3.0, 6.0, 12.0 })
    {
        REQUIRE (button.onClick != nullptr);
        button.onClick();
        INFO ("expected " << expected);
        CHECK_THAT (f.processor.getDisplayRangeDb(), WithinAbs (expected, 0.0));
        CHECK_THAT (display.getAxis().getRangeDb(), WithinAbs (expected, 0.0));
        CHECK (button.getButtonText() == juce::String (juce::roundToInt (expected)) + " dB");
    }
}

TEST_CASE ("The display recomputes curves only after a change", "[editor]")
{
    EditorFixture f;
    auto& display = f.editor.getDisplay();

    f.editor.refreshControls();
    const auto baseline = display.getCurves().getNumRecomputes();

    f.editor.refreshControls();
    f.editor.refreshControls();
    CHECK (display.getCurves().getNumRecomputes() == baseline);

    setBand (f.processor, 4, FilterType::bell, 500.0f, 6.0f, 1.0f, 3, true);
    f.editor.refreshControls();
    CHECK (display.getCurves().getNumRecomputes() == baseline + 1);
    CHECK (display.getCurves().isBandActive (3));
}

TEST_CASE ("Curves are clipped to the plot area", "[editor]")
{
    EditorFixture f;
    auto& display = f.editor.getDisplay();

    // +30 dB on a 3 dB display would run far outside the plot without clipping.
    setBand (f.processor, 4, FilterType::bell, 500.0f, 30.0f, 1.0f, 3, true);
    setBand (f.processor, 1, FilterType::lowCut, 200.0f, 0.0f, 0.71f, 15, true);
    f.processor.setDisplayRangeDb (3.0);
    f.editor.refreshControls();

    const auto plot = display.getPlotArea();
    const auto path = display.getSumPath().getBounds();
    INFO ("path " << path.toString() << " plot " << plot.toString());
    CHECK_FALSE (path.isEmpty());
    CHECK (plot.expanded (1.0f).contains (path));
}

TEST_CASE ("Editors can be opened and closed repeatedly", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor processor;

    // JUCE's leak detector fails the run if a component outlives this.
    for (int i = 0; i < 10; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editor->setSize (1100, 700);
    }
}

TEST_CASE ("Dynamic controls follow the band and its parameters", "[editor][dynamics]")
{
    EditorFixture f;
    auto& panel = f.editor.getBandPanel();
    auto& p = f.processor;

    for (int band : { 2, 11 })
    {
        INFO ("band " << band);
        setBand (p, band, FilterType::bell, 1000.0f, 0.0f, 1.0f, 3, true);
        panel.setBand (band);

        // Control -> parameter.
        panel.getDynamicButton().setToggleState (true, juce::sendNotificationSync);
        panel.getDynamicModeBox().setSelectedItemIndex (1, juce::sendNotificationSync);
        panel.getDetectorBox().setSelectedItemIndex (1, juce::sendNotificationSync);
        panel.getSidechainButton().setToggleState (true, juce::sendNotificationSync);
        panel.getThresholdSlider().setValue (-33.0, juce::sendNotificationSync);
        panel.getRangeSlider().setValue (9.0, juce::sendNotificationSync);
        panel.getRatioSlider().setValue (4.0, juce::sendNotificationSync);
        panel.getAttackSlider().setValue (3.0, juce::sendNotificationSync);
        panel.getReleaseSlider().setValue (400.0, juce::sendNotificationSync);

        CHECK_THAT (value (p, Parameters::id (band, "dyn")), WithinAbs (1.0f, 0.0f));
        CHECK_THAT (value (p, Parameters::id (band, "dynmode")), WithinAbs (1.0f, 0.0f));
        CHECK_THAT (value (p, Parameters::id (band, "detector")), WithinAbs (1.0f, 0.0f));
        CHECK_THAT (value (p, Parameters::id (band, "sidechain")), WithinAbs (1.0f, 0.0f));
        CHECK_THAT (value (p, Parameters::id (band, "thresh")), WithinAbs (-33.0f, 0.01f));
        CHECK_THAT (value (p, Parameters::id (band, "range")), WithinAbs (9.0f, 0.01f));
        CHECK_THAT (value (p, Parameters::id (band, "ratio")), WithinRel (4.0f, 1e-3f));
        CHECK_THAT (value (p, Parameters::id (band, "attack")), WithinRel (3.0f, 1e-3f));
        CHECK_THAT (value (p, Parameters::id (band, "release")), WithinRel (400.0f, 1e-3f));

        // Parameter -> control.
        set (p, Parameters::id (band, "thresh"), -12.0f);
        set (p, Parameters::id (band, "dynmode"), 0.0f);
        set (p, Parameters::id (band, "dyn"), 0.0f);
        CHECK_THAT (panel.getThresholdSlider().getValue(), WithinAbs (-12.0, 0.01));
        CHECK (panel.getDynamicModeBox().getSelectedItemIndex() == 0);
        CHECK_FALSE (panel.getDynamicButton().getToggleState());
    }

    // Switching bands must not write to the band left behind.
    panel.setBand (5);
    const auto before = value (p, Parameters::id (11, "thresh"));
    panel.getThresholdSlider().setValue (-50.0, juce::sendNotificationSync);
    CHECK_THAT (value (p, Parameters::id (11, "thresh")), WithinAbs (before, 0.0f));
    CHECK_THAT (value (p, Parameters::id (5, "thresh")), WithinAbs (-50.0f, 0.01f));
}

TEST_CASE ("Dynamic controls are active only where dynamics apply", "[editor][dynamics]")
{
    EditorFixture f;
    auto& panel = f.editor.getBandPanel();
    auto& p = f.processor;
    setBand (p, 3, FilterType::bell, 500.0f, 0.0f, 1.0f, 3, true);
    panel.setBand (3);

    auto settings = [&] { return std::vector<juce::Component*> { &panel.getDynamicModeBox(), &panel.getDetectorBox(),
                                                                 &panel.getSidechainButton(), &panel.getThresholdSlider(),
                                                                 &panel.getRangeSlider(), &panel.getAttackSlider(),
                                                                 &panel.getReleaseSlider() }; };

    // Type: only Bell and the shelves can be dynamic.
    set (p, Parameters::id (3, "dyn"), 1.0f);
    for (int t = 0; t < FilterTypes::count; ++t)
    {
        const auto type = static_cast<FilterType> (t);
        set (p, Parameters::id (3, "type"), static_cast<float> (t));
        f.editor.refreshControls();
        INFO ("type " << FilterTypes::names[t]);
        CHECK (panel.getDynamicButton().isEnabled() == BandSettings::typeCanBeDynamic (type));
        for (auto* c : settings())
            CHECK (c->isEnabled() == BandSettings::typeCanBeDynamic (type));
    }

    // Dynamics off: the switch stays usable, the settings grey out.
    set (p, Parameters::id (3, "type"), static_cast<float> (FilterType::bell));
    set (p, Parameters::id (3, "dyn"), 0.0f);
    f.editor.refreshControls();
    CHECK (panel.getDynamicButton().isEnabled());
    for (auto* c : settings())
        CHECK_FALSE (c->isEnabled());
    CHECK_FALSE (panel.getRatioSlider().isEnabled());

    // Ratio is used in Ratio mode only; Range caps both modes.
    set (p, Parameters::id (3, "dyn"), 1.0f);
    set (p, Parameters::id (3, "dynmode"), 0.0f);
    f.editor.refreshControls();
    CHECK_FALSE (panel.getRatioSlider().isEnabled());
    CHECK (panel.getRangeSlider().isEnabled());
    set (p, Parameters::id (3, "dynmode"), 1.0f);
    f.editor.refreshControls();
    CHECK (panel.getRatioSlider().isEnabled());

    // A disabled band: nothing but its On switch.
    set (p, Parameters::id (3, "enabled"), 0.0f);
    f.editor.refreshControls();
    CHECK_FALSE (panel.getDynamicButton().isEnabled());
    CHECK_FALSE (panel.getRatioSlider().isEnabled());
    for (auto* c : settings())
        CHECK_FALSE (c->isEnabled());
}

TEST_CASE ("Dynamic controls fit and stay readable at every size", "[editor][dynamics]")
{
    EditorFixture f;
    auto& panel = f.editor.getBandPanel();
    setBand (f.processor, 4, FilterType::bell, 500.0f, 3.0f, 1.0f, 3, true);
    set (f.processor, "band4_dyn", 1.0f);
    f.editor.refreshControls();
    f.editor.getDisplay().setSelection ({ 4 }, 4);

    REQUIRE (panel.getDynamicModeBox().getNumItems() == 2);
    REQUIRE (panel.getDetectorBox().getNumItems() == 2);

    for (auto [w, h] : sizes)
    {
        f.editor.setSize (w, h);
        INFO ("size " << w << "x" << h);
        checkLaidOut (f.editor, f.editor, "editor");

        for (auto* knob : { &panel.getThresholdSlider(), &panel.getRangeSlider(), &panel.getRatioSlider(),
                            &panel.getAttackSlider(), &panel.getReleaseSlider() })
        {
            CHECK (knob->getWidth() >= 30);
            CHECK (knob->getHeight() >= 30);
        }

        for (int i = 0; i < 2; ++i)
        {
            set (f.processor, "band4_dynmode", static_cast<float> (i));
            set (f.processor, "band4_detector", static_cast<float> (i));
            CHECK (menuReadable (panel.getDynamicModeBox(), i));
            CHECK (menuReadable (panel.getDetectorBox(), i));
        }
    }
}

TEST_CASE ("The display draws a dynamic band at its live gain", "[editor][dynamics]")
{
    EditorFixture f;
    constexpr double fs = 48000.0;
    setBand (f.processor, 6, FilterType::bell, 1000.0f, 2.0f, 1.0f, 3, true);
    set (f.processor, "band6_dyn", 1.0f);
    set (f.processor, "band6_thresh", -30.0f);
    set (f.processor, "band6_range", -6.0f);
    f.processor.setPlayConfigDetails (2, 2, fs, 512);
    f.processor.prepareToPlay (fs, 512);

    // A loud 1 kHz tone drives the band to its full range.
    juce::MidiBuffer midi;
    for (int block = 0; block < 100; ++block)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        for (int i = 0; i < 512; ++i)
            for (int ch = 0; ch < 2; ++ch)
                buffer.setSample (ch, i, 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 1000.0f * static_cast<float> (block * 512 + i) / 48000.0f));
        f.processor.processBlock (buffer, midi);
    }

    const auto live = f.processor.getLiveGainChangeDb (6, 0);
    REQUIRE (live < -5.0f);
    f.editor.refreshControls();

    const auto& curves = f.editor.getDisplay().getCurves();
    REQUIRE (curves.isBandDynamic (5));
    auto settings = f.processor.getBandSettings()[5];
    settings.gainDb += live;

    int k = 0;
    while (curves.frequency (k) < 1000.0)
        ++k;
    CHECK_THAT (curves.bandDb (5, k), WithinAbs (BandDesign::design (settings, fs).magnitudeDb (curves.frequency (k), fs), 0.05));
}

TEST_CASE ("Phase mode menus drive the processor and follow it", "[editor][linearphase]")
{
    EditorFixture f;
    auto& p = f.processor;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    f.editor.refreshControls();

    auto& controls = f.editor.getTopBar().getPhaseModeControls();
    auto& mode = controls.getModeBox();
    auto& length = controls.getLengthBox();

    REQUIRE (mode.getNumItems() == 2);
    REQUIRE (length.getNumItems() == 3);
    CHECK (mode.getSelectedItemIndex() == 0);
    CHECK_FALSE (length.isEnabled());   // no length in Zero latency mode

    // Lengths are shown as their latency at the current sample rate (taps / 2 + 512 samples).
    CHECK (length.getItemText (0) == "96 ms");
    CHECK (length.getItemText (1) == "181 ms");
    CHECK (length.getItemText (2) == "352 ms");

    // Control -> processor.
    mode.setSelectedItemIndex (1, juce::sendNotificationSync);
    CHECK (p.isLinearPhase());
    CHECK (length.isEnabled());
    length.setSelectedItemIndex (2, juce::sendNotificationSync);
    CHECK (p.getLinearPhaseLength() == 2);
    CHECK (p.getLatencySamples() == 16384 + 512);

    // Processor (e.g. a loaded session) -> controls.
    p.setLinearPhase (false);
    p.setLinearPhaseLength (1);
    f.editor.refreshControls();
    CHECK (mode.getSelectedItemIndex() == 0);
    CHECK (length.getSelectedItemIndex() == 1);
    CHECK_FALSE (length.isEnabled());

    // Another sample rate (a host sets the rate, then prepares): the latencies in ms follow.
    p.setPlayConfigDetails (2, 2, 96000.0, 512);
    p.prepareToPlay (96000.0, 512);
    f.editor.refreshControls();
    CHECK (length.getItemText (0) == "48 ms");
    CHECK (length.getItemText (2) == "176 ms");
}

TEST_CASE ("Phase mode menus fit the top bar beside the preset browser", "[editor][linearphase]")
{
    EditorFixture f;
    auto& bar = f.editor.getTopBar();
    auto& controls = bar.getPhaseModeControls();

    for (auto [w, h] : sizes)
    {
        f.editor.setSize (w, h);
        INFO ("size " << w << "x" << h);
        checkLaidOut (f.editor, f.editor, "editor");

        const auto controlsArea = inEditor (controls, f.editor);
        CHECK (inEditor (bar, f.editor).contains (controlsArea));
        CHECK_FALSE (controlsArea.intersects (inEditor (bar.getNextButton(), f.editor)));
        CHECK (controlsArea.getX() > inEditor (bar.getNextButton(), f.editor).getRight());

        for (int i = 0; i < 2; ++i)
        {
            f.processor.setLinearPhase (i == 1);
            f.editor.refreshControls();
            CHECK (menuReadable (controls.getModeBox(), i));
        }
        for (int i = 0; i < 3; ++i)
        {
            f.processor.setLinearPhaseLength (i);
            f.editor.refreshControls();
            CHECK (menuReadable (controls.getLengthBox(), i));
        }
    }
}

TEST_CASE ("Editor snapshot (renders PNGs to $EQ_SNAPSHOT_DIR)", "[.snapshot]")
{
    // Hidden: run with  EQ_SNAPSHOT_DIR=<dir> SpectralFaultTests "[.snapshot]"  to look at the layout.
    const auto dir = juce::SystemStats::getEnvironmentVariable ("EQ_SNAPSHOT_DIR", {});
    if (dir.isEmpty())
        SKIP ("EQ_SNAPSHOT_DIR not set");

    EditorFixture f;
    setBand (f.processor, 1, FilterType::lowCut, 30.0f, 0.0f, 0.71f, 3, true);
    setBand (f.processor, 3, FilterType::bell, 90.0f, 5.0f, 3.0f, 3, true);
    setBand (f.processor, 5, FilterType::notch, 170.0f, 0.0f, 6.0f, 3, true);
    setBand (f.processor, 8, FilterType::bell, 800.0f, 9.0f, 0.8f, 3, true);
    setBand (f.processor, 10, FilterType::bell, 2500.0f, -6.0f, 1.2f, 3, true);
    setBand (f.processor, 15, FilterType::highShelf, 9000.0f, 3.0f, 0.71f, 3, true);
    setBand (f.processor, 12, FilterType::bell, 5500.0f, 6.0f, 2.0f, 3, false);   // disabled: grey
    set (f.processor, Parameters::id (10, "channel"), static_cast<float> (ChannelMode::left));
    set (f.processor, Parameters::id (15, "channel"), static_cast<float> (ChannelMode::side));
    set (f.processor, Parameters::id (8, "dyn"), 1.0f);           // dynamic: range and live gain drawn
    set (f.processor, Parameters::id (8, "thresh"), -40.0f);
    set (f.processor, Parameters::id (8, "range"), -8.0f);
    f.processor.setLinearPhase (true);                             // phase menus show Linear phase
    setBand (f.processor, 14, FilterType::highCut, 6000.0f, 0.0f, 0.71f, 5, true);   // post drops above 6 kHz
    f.editor.refreshControls();
    f.editor.getDisplay().setSelection ({ 8, 10 }, 8);

    // Some audio through the plugin, so the analyzer and meter have something to show:
    // noise with a falling (roughly pink) spectrum, from a one-pole lowpass on white noise.
    f.processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    f.processor.prepareToPlay (48000.0, 512);
    juce::Random random (9);
    float state = 0.0f;
    juce::MidiBuffer midi;
    auto feed = [&]
    {
        for (int block = 0; block < 32; ++block)
        {
            juce::AudioBuffer<float> buffer (2, 512);
            for (int i = 0; i < 512; ++i)
            {
                state = 0.97f * state + 0.03f * (random.nextFloat() * 2.0f - 1.0f);
                const auto x = 0.25f * (0.5f * state * 8.0f + 0.1f * (random.nextFloat() * 2.0f - 1.0f));
                buffer.setSample (0, i, x);
                buffer.setSample (1, i, x);
            }
            f.processor.processBlock (buffer, midi);
        }
        f.editor.refreshControls();
    };

    for (auto [w, h] : sizes)
    {
        f.editor.setSize (w, h);
        feed();
        f.editor.getDisplay().handleHover (f.editor.getDisplay().getAxis().getPlotArea().getRelativePoint (0.42f, 0.5f));   // a peak marker, if one is near
        const auto image = f.editor.createComponentSnapshot (f.editor.getLocalBounds());
        juce::File file (dir + "/m3_" + juce::String (w) + "x" + juce::String (h) + ".png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
    }

    // An EQ Sketch in progress (Option-drag, not released), at the default size.
    f.editor.setSize (E::defaultWidth, E::defaultHeight);
    auto& display = f.editor.getDisplay();
    const auto axis = display.getAxis();
    const juce::ModifierKeys option { juce::ModifierKeys::altModifier };
    display.handlePress ({ axis.xForFrequency (300.0), axis.yForDb (0.0) }, option, 1);
    for (int i = 1; i <= 80; ++i)
    {
        const auto t = i / 80.0;
        display.handleDrag ({ axis.xForFrequency (300.0 * std::pow (20.0, t)), axis.yForDb (5.0 * std::sin (6.28318 * t)) }, option);
    }
    // EQ Match panel contents, rendered on their own.
    {
        auto& panel = f.editor.getMatchPanel();
        panel.setSize (380, 210);
        const auto panelImage = panel.createComponentSnapshot (panel.getLocalBounds());
        juce::File panelFile (dir + "/m9e_match_panel.png");
        panelFile.deleteFile();
        juce::FileOutputStream panelStream (panelFile);
        REQUIRE (juce::PNGImageFormat().writeImageToStream (panelImage, panelStream));
    }

    const auto sketchImage = f.editor.createComponentSnapshot (f.editor.getLocalBounds());
    juce::File sketchFile (dir + "/m9d_sketch.png");
    sketchFile.deleteFile();
    juce::FileOutputStream sketchStream (sketchFile);
    REQUIRE (juce::PNGImageFormat().writeImageToStream (sketchImage, sketchStream));
}
