// Renders the documentation screenshots (README, user guide) into $EQ_SCREENSHOT_DIR.
// Hidden: run with  EQ_SCREENSHOT_DIR=docs/images build/tests/SpectralFaultTests "[.screenshots]"

#include "TestParameters.h"

#include "PluginEditor.h"
#include "dsp/BandDesign.h"
#include "dsp/CascadeProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace TestParameters;

namespace
{
    constexpr double fs = 48000.0;
    constexpr float scale = 2.0f;   // sharp on high-resolution screens

    /** Music-like test audio: pink-ish noise, a bass line with harmonics, a pad and a bright resonance. */
    struct MusicLike
    {
        juce::Random random { 21 };
        double b0 = 0, b1 = 0, b2 = 0, phase = 0, t = 0;
        float resonanceState1 = 0, resonanceState2 = 0;

        float next (double resonanceHz = 2800.0, float resonanceAmount = 0.4f)
        {
            const auto white = random.nextFloat() * 2.0 - 1.0;
            b0 = 0.99765 * b0 + white * 0.0990460;
            b1 = 0.96300 * b1 + white * 0.2965164;
            b2 = 0.57000 * b2 + white * 1.0526913;
            const auto pink = (b0 + b1 + b2 + white * 0.1848) * 0.05;

            t += 1.0 / fs;
            const auto note = std::fmod (t, 2.0) < 1.0 ? 55.0 : 73.4;   // A1, D2
            phase += 2.0 * juce::MathConstants<double>::pi * note / fs;
            double bass = 0.0;
            for (int h = 1; h <= 6; ++h)
                bass += std::sin (h * phase) / (h * h);
            const auto pad = 0.05 * (std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * t)
                                     + std::sin (2.0 * juce::MathConstants<double>::pi * 554.4 * t)
                                     + std::sin (2.0 * juce::MathConstants<double>::pi * 659.3 * t));

            // A resonance: a two-pole band pass on the noise.
            const auto w = 2.0 * juce::MathConstants<double>::pi * resonanceHz / fs;
            const auto r = 0.995;
            const auto y = static_cast<float> (white * 0.02 + 2.0 * r * std::cos (w) * resonanceState1 - r * r * resonanceState2);
            resonanceState2 = resonanceState1;
            resonanceState1 = y;

            return static_cast<float> (pink + 0.12 * bass + pad) + resonanceAmount * y;
        }
    };

    struct Scene
    {
        juce::ScopedJuceInitialiser_GUI juce;
        ParametricEQAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> base;
        ParametricEQAudioProcessorEditor* editor = nullptr;
        MusicLike music;

        Scene()
        {
            processor.setPlayConfigDetails (2, 2, fs, 512);
            processor.prepareToPlay (fs, 512);
            base.reset (processor.createEditor());
            editor = dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());
            editor->setSize (ParametricEQAudioProcessorEditor::defaultWidth, ParametricEQAudioProcessorEditor::defaultHeight);
        }

        /** Plays audio (stereo, the right channel slightly different) so the analyzer and meter settle. */
        /** Optional colouring of the played audio (e.g. a brighter "reference"). */
        std::vector<BandSettings> colour;

        void play (double seconds, double resonanceHz = 2800.0, float resonanceAmount = 0.4f, float sideAmount = 0.15f)
        {
            std::vector<CascadeProcessor> filters (colour.size());
            for (size_t k = 0; k < colour.size(); ++k)
                filters[k].setCoefficients (BandDesign::design (colour[k], fs));

            juce::MidiBuffer midi;
            const auto blocks = static_cast<int> (seconds * fs / 512.0);
            for (int block = 0; block < blocks; ++block)
            {
                juce::AudioBuffer<float> b (2, 512);
                for (int i = 0; i < 512; ++i)
                {
                    auto x = music.next (resonanceHz, resonanceAmount);
                    for (auto& f : filters)
                        x = static_cast<float> (f.processSample (0, x));
                    const auto side = sideAmount * (music.random.nextFloat() - 0.5f) * 0.2f;
                    b.setSample (0, i, x + side);
                    b.setSample (1, i, x - side);
                }
                processor.processBlock (b, midi);
                editor->refreshControls();
            }
        }

        juce::Point<float> at (double frequencyHz, double gainDb) const
        {
            const auto axis = editor->getDisplay().getAxis();
            return { axis.xForFrequency (frequencyHz), axis.yForDb (gainDb) };
        }

        void save (juce::Component& c, const juce::String& name) const
        {
            const auto dir = juce::SystemStats::getEnvironmentVariable ("EQ_SCREENSHOT_DIR", {});
            const auto image = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
            juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (dir).getChildFile (name + ".png"));
            file.deleteFile();
            juce::FileOutputStream stream (file);
            REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
        }

        /** A typical mix-ready curve. */
        void typicalBands()
        {
            setBand (processor, 1, FilterType::lowCut, 32.0f, 0.0f, 0.71f, 3, true);
            setBand (processor, 3, FilterType::lowShelf, 110.0f, 2.0f, 0.71f, 3, true);
            setBand (processor, 5, FilterType::bell, 340.0f, -3.5f, 1.6f, 3, true);
            setBand (processor, 8, FilterType::bell, 2800.0f, -5.0f, 6.0f, 3, true);
            setBand (processor, 10, FilterType::bell, 4500.0f, 2.5f, 0.9f, 3, true);
            setBand (processor, 13, FilterType::highShelf, 10000.0f, 2.0f, 0.71f, 3, true);
            setBand (processor, 16, FilterType::highCut, 19000.0f, 0.0f, 0.71f, 1, true);
        }
    };
}

TEST_CASE ("Documentation screenshots (renders PNGs to $EQ_SCREENSHOT_DIR)", "[.screenshots]")
{
    if (juce::SystemStats::getEnvironmentVariable ("EQ_SCREENSHOT_DIR", {}).isEmpty())
        SKIP ("EQ_SCREENSHOT_DIR not set");

    SECTION ("hero: the full plugin with a typical curve and the analyzer")
    {
        Scene s;
        s.typicalBands();
        s.editor->getDisplay().setSelection ({ 10 }, 10);
        s.play (2.5);
        s.save (*s.editor, "hero");
    }

    SECTION ("dynamic band: live gain and range, with the dynamics controls")
    {
        Scene s;
        s.typicalBands();
        setBand (s.processor, 8, FilterType::bell, 2800.0f, -1.0f, 4.0f, 3, true);
        set (s.processor, "band8_dyn", 1.0f);
        set (s.processor, "band8_thresh", -42.0f);
        set (s.processor, "band8_range", -8.0f);
        set (s.processor, "band8_attack", 5.0f);
        s.editor->getDisplay().setSelection ({ 8 }, 8);
        s.play (2.5, 2800.0, 1.2f);
        s.save (*s.editor, "dynamic-band");
    }

    SECTION ("stereo: Left/Right and Mid/Side bands with split curves")
    {
        Scene s;
        setBand (s.processor, 2, FilterType::lowCut, 40.0f, 0.0f, 0.71f, 3, true);
        setBand (s.processor, 4, FilterType::bell, 250.0f, 3.0f, 1.2f, 3, true);
        set (s.processor, "band4_channel", static_cast<float> (ChannelMode::mid));
        setBand (s.processor, 6, FilterType::lowCut, 180.0f, 0.0f, 0.71f, 1, true);
        set (s.processor, "band6_channel", static_cast<float> (ChannelMode::side));
        setBand (s.processor, 9, FilterType::highShelf, 7000.0f, 4.0f, 0.71f, 3, true);
        set (s.processor, "band9_channel", static_cast<float> (ChannelMode::side));
        setBand (s.processor, 12, FilterType::bell, 1800.0f, -3.0f, 2.0f, 3, true);
        set (s.processor, "band12_channel", static_cast<float> (ChannelMode::mid));
        s.play (2.0, 2800.0, 0.2f, 1.0f);
        s.save (*s.editor, "stereo");
    }

    SECTION ("peak pick: rings on the most prominent peaks")
    {
        Scene s;
        setBand (s.processor, 1, FilterType::lowCut, 30.0f, 0.0f, 0.71f, 3, true);
        s.play (2.5, 2800.0, 0.6f);
        s.editor->getDisplay().handleHover (s.at (2400.0, 6.0));
        s.save (*s.editor, "peak-pick");
    }

    SECTION ("EQ Sketch: drawing a curve")
    {
        Scene s;
        setBand (s.processor, 1, FilterType::lowCut, 30.0f, 0.0f, 0.71f, 3, true);
        s.play (2.0, 2800.0, 0.2f);
        auto& display = s.editor->getDisplay();
        const juce::ModifierKeys option { juce::ModifierKeys::altModifier };
        display.handlePress (s.at (60.0, 3.0), option, 1);
        for (int i = 1; i <= 120; ++i)
        {
            const auto t = i / 120.0;
            const auto f = 60.0 * std::pow (16000.0 / 60.0, t);
            const auto db = 3.0 * (1.0 - t) - 4.0 * std::exp (-std::pow ((t - 0.45) / 0.12, 2.0)) + 3.5 * std::pow (t, 3.0);
            display.handleDrag (s.at (f, db), option);
        }
        s.save (*s.editor, "eq-sketch");
    }

    SECTION ("EQ Match: learned spectra, preview and the Match window")
    {
        // The reference: the same kind of material, brighter and a little leaner in the low mids.
        Scene s;
        auto& session = s.processor.getMatchSession();
        auto shape = [] (FilterType type, double f, double gain, double q)
        {
            BandSettings b;
            b.type = type; b.frequencyHz = f; b.gainDb = gain; b.q = q;
            return b;
        };
        s.colour = { shape (FilterType::highShelf, 5000.0, 4.0, 0.71), shape (FilterType::bell, 300.0, -3.0, 1.0) };
        session.startLearning (MatchSession::Learning::reference);
        s.play (4.0, 2800.0, 0.2f);
        s.colour.clear();
        session.startLearning (MatchSession::Learning::current);
        s.play (4.0, 2800.0, 0.2f);
        session.stopLearning();
        session.setSmoothingOctaves (1.0 / 2.0);
        s.editor->setMatchWindowOpen (true);
        s.editor->refreshControls();
        s.save (*s.editor, "eq-match-preview");

        auto& panel = s.editor->getMatchPanel();
        panel.refresh();
        s.save (panel, "eq-match-window");
        s.editor->setMatchWindowOpen (false);
    }

    SECTION ("top bar: undo/redo, A/B, presets, phase mode")
    {
        Scene s;
        s.typicalBands();
        auto& gain = param (s.processor, "band10_gain");
        gain.beginChangeGesture();
        gain.setValueNotifyingHost (gain.convertTo0to1 (3.0f));
        gain.endChangeGesture();
        s.processor.setLinearPhase (true);
        s.editor->refreshControls();
        s.save (s.editor->getTopBar(), "top-bar");
    }
}
