#include "TestParameters.h"

#include "PluginEditor.h"
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
    CHECK (f.editor.getBandPanel().isVisible());
    CHECK (f.editor.getBottomBar().isVisible());
}

TEST_CASE ("Layout follows the planned arrangement at every size", "[editor]")
{
    EditorFixture f;

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

TEST_CASE ("The band panel has 16 tabs and follows the selected band", "[editor]")
{
    EditorFixture f;
    auto& panel = f.editor.getBandPanel();
    auto& p = f.processor;

    CHECK (panel.getBand() == 1);
    CHECK (panel.getTab (1).getToggleState());

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        INFO ("band " << band);
        // triggerClick() is asynchronous; call the click handler directly.
        REQUIRE (panel.getTab (band).onClick != nullptr);
        panel.getTab (band).onClick();
        CHECK (panel.getBand() == band);

        for (int other = 1; other <= Parameters::numBands; ++other)
            CHECK (panel.getTab (other).getToggleState() == (other == band));

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

TEST_CASE ("Band tabs carry their band's colour", "[editor]")
{
    EditorFixture f;
    for (int band = 1; band <= Parameters::numBands; ++band)
        CHECK (f.editor.getBandPanel().getTab (band).findColour (juce::TextButton::textColourOffId)
               == ResponseDisplay::bandColour (band));
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

TEST_CASE ("Editor snapshot (renders PNGs to $EQ_SNAPSHOT_DIR)", "[.snapshot]")
{
    // Hidden: run with  EQ_SNAPSHOT_DIR=<dir> ParametricEQTests "[.snapshot]"  to look at the layout.
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
    f.editor.getBandPanel().setBand (8);
    f.editor.refreshControls();

    for (auto [w, h] : sizes)
    {
        f.editor.setSize (w, h);
        f.editor.refreshControls();
        const auto image = f.editor.createComponentSnapshot (f.editor.getLocalBounds());
        juce::File file (dir + "/m3_" + juce::String (w) + "x" + juce::String (h) + ".png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
    }
}
