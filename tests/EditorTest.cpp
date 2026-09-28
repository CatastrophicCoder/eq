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

    /** Every visible child sits inside the given area and has a usable size. */
    void checkLaidOut (juce::Component& c, juce::Rectangle<int> area, const juce::String& path)
    {
        for (auto* child : c.getChildren())
        {
            if (! child->isVisible())
                continue;

            const auto bounds = child->getBounds() + c.getScreenPosition() - c.getTopLevelComponent()->getScreenPosition();
            INFO (path << "/" << child->getName() << " " << bounds.toString() << " in " << area.toString());
            CHECK_FALSE (child->getBounds().isEmpty());
            CHECK (area.contains (bounds));

            checkLaidOut (*child, area, path + "/" + child->getName());
        }
    }
}

TEST_CASE ("Editor has 16 band strips and an output strip at the planned size", "[editor]")
{
    EditorFixture f;

    CHECK (f.editor.getWidth() == ParametricEQAudioProcessorEditor::defaultWidth);
    CHECK (f.editor.getHeight() == ParametricEQAudioProcessorEditor::defaultHeight);
    CHECK (f.editor.isResizable());

    auto* constrainer = f.editor.getConstrainer();
    REQUIRE (constrainer != nullptr);
    CHECK (constrainer->getMinimumWidth() == ParametricEQAudioProcessorEditor::minWidth);
    CHECK (constrainer->getMinimumHeight() == ParametricEQAudioProcessorEditor::minHeight);
    CHECK (constrainer->getMaximumWidth() == ParametricEQAudioProcessorEditor::maxWidth);
    CHECK (constrainer->getMaximumHeight() == ParametricEQAudioProcessorEditor::maxHeight);

    for (int band = 1; band <= Parameters::numBands; ++band)
        CHECK (f.editor.getBandStrip (band).isVisible());
    CHECK (f.editor.getOutputStrip().isVisible());
}

TEST_CASE ("Editor lays out every control inside the window at minimum, default and maximum size", "[editor]")
{
    EditorFixture f;
    using E = ParametricEQAudioProcessorEditor;

    for (auto [w, h] : { std::pair { E::minWidth, E::minHeight }, { E::defaultWidth, E::defaultHeight },
                         { E::maxWidth, E::maxHeight } })
    {
        f.editor.setSize (w, h);
        INFO ("size " << w << "x" << h);

        checkLaidOut (f.editor, f.editor.getLocalBounds(), "editor");

        // Knobs stay usable at the smallest size.
        for (int band = 1; band <= Parameters::numBands; ++band)
        {
            auto& strip = f.editor.getBandStrip (band);
            for (auto* knob : { &strip.getFrequencySlider(), &strip.getGainSlider(), &strip.getQSlider() })
            {
                INFO ("band " << band);
                CHECK (knob->getWidth() >= 30);
                CHECK (knob->getHeight() >= 30);
            }
        }

        // Strips do not overlap each other.
        for (int band = 1; band < Parameters::numBands; ++band)
            CHECK (f.editor.getBandStrip (band).getRight() <= f.editor.getBandStrip (band + 1).getX());
        CHECK (f.editor.getBandStrip (Parameters::numBands).getRight() <= f.editor.getOutputStrip().getX());
    }
}

TEST_CASE ("Band controls are attached to their parameters in both directions", "[editor]")
{
    EditorFixture f;
    auto& p = f.processor;

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        auto& strip = f.editor.getBandStrip (band);
        INFO ("band " << band);

        // Control -> parameter.
        strip.getFrequencySlider().setValue (2345.0, juce::sendNotificationSync);
        strip.getGainSlider().setValue (-7.25, juce::sendNotificationSync);
        strip.getQSlider().setValue (3.3, juce::sendNotificationSync);
        strip.getTypeBox().setSelectedItemIndex (static_cast<int> (FilterType::notch), juce::sendNotificationSync);
        strip.getSlopeBox().setSelectedItemIndex (CutSlope::brickwallIndex, juce::sendNotificationSync);
        strip.getEnableButton().setToggleState (true, juce::sendNotificationSync);

        CHECK_THAT (value (p, Parameters::id (band, "freq")), WithinRel (2345.0f, 1e-3f));
        CHECK_THAT (value (p, Parameters::id (band, "gain")), WithinAbs (-7.25f, 0.01f));
        CHECK_THAT (value (p, Parameters::id (band, "q")), WithinRel (3.3f, 1e-3f));
        CHECK_THAT (value (p, Parameters::id (band, "type")), WithinAbs (static_cast<float> (FilterType::notch), 0.0f));
        CHECK_THAT (value (p, Parameters::id (band, "slope")), WithinAbs (static_cast<float> (CutSlope::brickwallIndex), 0.0f));
        CHECK_THAT (value (p, Parameters::id (band, "enabled")), WithinAbs (1.0f, 0.0f));

        // Parameter -> control.
        setBand (p, band, FilterType::highShelf, 432.0f, 11.5f, 0.9f, 5, false);
        CHECK_THAT (strip.getFrequencySlider().getValue(), WithinRel (432.0, 1e-3));
        CHECK_THAT (strip.getGainSlider().getValue(), WithinAbs (11.5, 0.01));
        CHECK_THAT (strip.getQSlider().getValue(), WithinRel (0.9, 1e-3));
        CHECK (strip.getTypeBox().getSelectedItemIndex() == static_cast<int> (FilterType::highShelf));
        CHECK (strip.getSlopeBox().getSelectedItemIndex() == 5);
        CHECK_FALSE (strip.getEnableButton().getToggleState());
    }
}

TEST_CASE ("Type and slope menus list the parameter choices in order", "[editor]")
{
    EditorFixture f;
    auto& strip = f.editor.getBandStrip (7);

    REQUIRE (strip.getTypeBox().getNumItems() == FilterTypes::count);
    for (int i = 0; i < FilterTypes::count; ++i)
        CHECK (strip.getTypeBox().getItemText (i) == FilterTypes::names[i]);

    REQUIRE (strip.getSlopeBox().getNumItems() == CutSlope::count);
    for (int i = 0; i < CutSlope::count; ++i)
        CHECK (strip.getSlopeBox().getItemText (i) == CutSlope::labels[i]);
}

TEST_CASE ("Output controls are attached to their parameters", "[editor]")
{
    EditorFixture f;
    auto& p = f.processor;
    auto& out = f.editor.getOutputStrip();

    out.getGainSlider().setValue (-4.5, juce::sendNotificationSync);
    out.getAutoGainButton().setToggleState (true, juce::sendNotificationSync);
    out.getInvertButton().setToggleState (true, juce::sendNotificationSync);

    CHECK_THAT (value (p, Parameters::outputGain), WithinAbs (-4.5f, 1e-4f));
    CHECK_THAT (value (p, Parameters::autoGain), WithinAbs (1.0f, 0.0f));
    CHECK_THAT (value (p, Parameters::outputInvert), WithinAbs (1.0f, 0.0f));

    set (p, Parameters::outputGain, 7.0f);
    set (p, Parameters::autoGain, 0.0f);
    set (p, Parameters::outputInvert, 0.0f);

    CHECK_THAT (out.getGainSlider().getValue(), WithinAbs (7.0, 1e-4));
    CHECK_FALSE (out.getAutoGainButton().getToggleState());
    CHECK_FALSE (out.getInvertButton().getToggleState());
}

TEST_CASE ("Controls a type does not use are greyed out", "[editor]")
{
    EditorFixture f;
    auto& p = f.processor;
    auto& strip = f.editor.getBandStrip (3);

    for (int t = 0; t < FilterTypes::count; ++t)
    {
        const auto type = static_cast<FilterType> (t);
        set (p, Parameters::id (3, "type"), static_cast<float> (t));
        f.editor.refreshControls();

        INFO ("type " << FilterTypes::names[t]);
        CHECK (strip.getGainSlider().isEnabled() == FilterTypes::usesGain (type));
        CHECK (strip.getQSlider().isEnabled() == FilterTypes::usesQ (type));
        CHECK (strip.getSlopeBox().isEnabled() == FilterTypes::usesSlope (type));
        CHECK (strip.getFrequencySlider().isEnabled());
        CHECK (strip.getTypeBox().isEnabled());
        CHECK (strip.getEnableButton().isEnabled());
    }
}

TEST_CASE ("The output strip shows the current Auto Gain correction", "[editor]")
{
    EditorFixture f;
    auto& p = f.processor;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);

    setBand (p, 5, FilterType::bell, 1000.0f, 9.0f, 1.0f, 3, true);
    set (p, Parameters::autoGain, 1.0f);

    for (int i = 0; i < 200 && std::abs (p.getAutoGainOffsetDb()) < 0.5f; ++i)
        juce::Thread::sleep (10);
    REQUIRE (std::abs (p.getAutoGainOffsetDb()) > 0.5f);

    f.editor.refreshControls();
    const auto expected = juce::String (p.getAutoGainOffsetDb(), 1) + " dB";
    INFO ("label: " << f.editor.getOutputStrip().getOffsetLabel().getText());
    CHECK (f.editor.getOutputStrip().getOffsetLabel().getText().contains (expected));
}

TEST_CASE ("Editors can be opened and closed repeatedly", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor processor;

    // JUCE's leak detector fails the run if a component outlives this.
    for (int i = 0; i < 10; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editor->setSize (1200, 400);
    }
}
