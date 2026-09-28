#include "TestParameters.h"

#include "PluginEditor.h"
#include "dsp/BandDesign.h"
#include "presets/FactoryPresets.h"
#include "presets/PresetManager.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <map>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace TestParameters;

namespace
{
    /** A temporary user-preset folder, removed afterwards. */
    struct TempFolder
    {
        juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("SpectralFaultTests-" + juce::Uuid().toString());
        TempFolder()  { dir.createDirectory(); }
        ~TempFolder() { dir.deleteRecursively(); }
    };

    struct Fixture
    {
        juce::ScopedJuceInitialiser_GUI juce;
        TempFolder folder;
        ParametricEQAudioProcessor processor;
        PresetManager& presets = processor.getPresetManager();
        Fixture() { presets.setUserFolder (folder.dir); }
    };

    /** The agreed factory presets (decision 2026-09-28), written out so accidental edits fail. */
    struct Spec { const char* name; const char* category; std::vector<Preset::Band> bands; };

    Preset::Band lc (float f, int slope = 1) { return { true, true, FilterType::lowCut, f, 0.0f, 0.71f, slope, ChannelMode::stereo }; }
    Preset::Band hc (float f, int slope = 1) { return { true, true, FilterType::highCut, f, 0.0f, 0.71f, slope, ChannelMode::stereo }; }
    Preset::Band bell (float f, float g, float q) { return { true, true, FilterType::bell, f, g, q, 3, ChannelMode::stereo }; }
    Preset::Band hs (float f, float g) { return { true, true, FilterType::highShelf, f, g, 0.71f, 3, ChannelMode::stereo }; }

    const std::vector<Spec>& specs()
    {
        static const std::vector<Spec> s {
            { "Lead Vocal",      "Vocals",  { lc (90),  bell (350, -2, 1.0f), bell (3000, 1.5f, 1.4f), hs (12000, 2) } },
            { "Male Vocal",      "Vocals",  { lc (100), bell (350, -2, 1.0f), bell (3000, 1.5f, 1.4f), hs (12000, 2) } },
            { "Female Vocal",    "Vocals",  { lc (120), bell (500, -2, 1.0f), bell (4200, 1.5f, 1.4f), hs (12000, 2) } },
            { "Acoustic Guitar", "Guitars", { lc (80),  bell (250, -2.5f, 1.0f), bell (1200, -1.5f, 2.0f), bell (3000, 1.5f, 0.8f), hs (10000, 1.5f) } },
            { "Electric Clean",  "Guitars", { lc (80),  bell (250, -1.5f, 1.0f), bell (3000, 1.5f, 1.0f) } },
            { "Electric Rhythm", "Guitars", { lc (80),  bell (100, 1, 1.0f), bell (300, -3, 1.4f), bell (2500, -2, 1.0f), hc (12000) } },
            { "Bass DI",         "Bass",    { lc (30),  bell (90, 1.5f, 1.0f), bell (300, -1.5f, 1.0f), bell (900, 1.5f, 1.0f) } },
            { "Kick",            "Drums",   { lc (35),  bell (80, 2, 1.4f), bell (250, -3, 1.0f), bell (3500, 2, 1.4f) } },
            { "Snare",           "Drums",   { lc (80),  bell (400, -2, 1.4f), bell (5000, 1.5f, 1.0f) } },
            { "Overheads",       "Drums",   { lc (150), bell (4000, -1.5f, 1.0f), bell (10000, 1.5f, 0.7f) } },
            { "Mix Bus Polish",  "Mix",     { lc (30, 0), bell (250, -1, 0.7f), hs (12000, 1) } } };
        return s;
    }

    Preset sample()
    {
        Preset p;
        p.name = "My Test";
        p.category = "User";
        p.bands[0] = lc (60, 3);
        p.bands[4] = bell (1234.5f, -3.25f, 2.5f);
        p.bands[4].channel = ChannelMode::mid;
        p.bands[9] = hs (8000, 4);
        p.bands[9].enabled = false;
        p.outputGainDb = -2.5f;
        p.autoGain = true;
        p.invert = true;
        return p;
    }

    /** Counts gesture begin/end per parameter. */
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
}

//==============================================================================
TEST_CASE ("A preset survives XML and back", "[preset]")
{
    const auto original = sample();
    const auto xml = original.toXml();
    REQUIRE (xml != nullptr);
    CHECK (xml->hasTagName (Preset::xmlTag));
    CHECK (xml->getIntAttribute ("formatVersion") == Preset::formatVersion);

    const auto parsed = Preset::fromXml (*xml);
    REQUIRE (parsed.has_value());
    CHECK (parsed->name == "My Test");
    CHECK (parsed->hasSameSettingsAs (original));
    CHECK (parsed->bands[4].channel == ChannelMode::mid);
    CHECK_FALSE (parsed->bands[9].enabled);
    CHECK (parsed->bands[9].inUse);
    CHECK_FALSE (parsed->bands[2].inUse);
}

TEST_CASE ("Invalid preset XML is rejected; out-of-range values are clamped", "[preset]")
{
    CHECK_FALSE (Preset::fromXml (juce::XmlElement ("SomethingElse")).has_value());

    auto xml = sample().toXml();
    xml->setAttribute ("formatVersion", Preset::formatVersion + 1);
    CHECK_FALSE (Preset::fromXml (*xml).has_value());

    xml = sample().toXml();
    xml->removeAttribute ("formatVersion");
    CHECK_FALSE (Preset::fromXml (*xml).has_value());

    xml = sample().toXml();
    xml->setAttribute ("name", juce::String());
    CHECK_FALSE (Preset::fromXml (*xml).has_value());

    xml = sample().toXml();
    for (auto* band : xml->getChildIterator())
    {
        band->setAttribute ("freq", 99999.0);
        band->setAttribute ("gain", -80.0);
    }
    xml->setAttribute ("outputGain", 50.0);
    const auto clamped = Preset::fromXml (*xml);
    REQUIRE (clamped.has_value());
    CHECK_THAT (clamped->bands[4].frequencyHz, WithinAbs (20000.0f, 0.0f));
    CHECK_THAT (clamped->bands[4].gainDb, WithinAbs (-30.0f, 0.0f));
    CHECK_THAT (clamped->outputGainDb, WithinAbs (30.0f, 0.0f));

    xml = sample().toXml();
    xml->getChildElement (0)->setAttribute ("type", "Wobble");   // unknown type: that band is skipped
    const auto skipped = Preset::fromXml (*xml);
    REQUIRE (skipped.has_value());
    CHECK_FALSE (skipped->bands[0].inUse);
    CHECK (skipped->bands[4].inUse);
}

//==============================================================================
TEST_CASE ("The factory presets are the agreed eleven", "[preset][factory]")
{
    const auto& factory = FactoryPresets::all();
    REQUIRE (factory.size() == specs().size());
    CHECK (FactoryPresets::categories() == juce::StringArray { "Vocals", "Guitars", "Bass", "Drums", "Mix" });

    for (size_t i = 0; i < factory.size(); ++i)
    {
        const auto& p = factory[i];
        const auto& s = specs()[i];
        INFO (s.name);
        CHECK (p.name == s.name);
        CHECK (p.category == s.category);
        CHECK_THAT (p.outputGainDb, WithinAbs (0.0f, 0.0f));
        CHECK_FALSE (p.autoGain);
        CHECK_FALSE (p.invert);

        for (size_t b = 0; b < 16; ++b)
        {
            INFO ("band " << b + 1);
            if (b >= s.bands.size())
            {
                CHECK_FALSE (p.bands[b].inUse);
                continue;
            }

            const auto& want = s.bands[b];
            const auto& got = p.bands[b];
            CHECK (got.inUse);
            CHECK (got.enabled);
            CHECK (got.type == want.type);
            CHECK_THAT (got.frequencyHz, WithinRel (want.frequencyHz, 1e-6f));
            CHECK_THAT (got.gainDb, WithinAbs (want.gainDb, 1e-6f));
            if (FilterTypes::usesQ (want.type))
                CHECK_THAT (got.q, WithinRel (want.q, 1e-6f));
            if (FilterTypes::usesSlope (want.type))
                CHECK (got.slopeIndex == want.slopeIndex);
            CHECK (got.channel == ChannelMode::stereo);
        }
    }
}

TEST_CASE ("Factory presets are valid: in range, stable, conservative", "[preset][factory]")
{
    for (const auto& p : FactoryPresets::all())
        for (const auto& b : p.bands)
        {
            if (! b.inUse)
                continue;

            INFO (p.name);
            CHECK (b.frequencyHz >= 20.0f);
            CHECK (b.frequencyHz <= 20000.0f);
            CHECK (std::abs (b.gainDb) <= 4.0f);    // starting points: moderate moves
            CHECK (b.q >= 0.1f);
            CHECK (b.q <= 18.0f);

            BandSettings s;
            s.type = b.type; s.frequencyHz = b.frequencyHz; s.gainDb = b.gainDb; s.q = b.q; s.slopeIndex = b.slopeIndex;
            for (auto fs : { 44100.0, 48000.0, 96000.0 })
                CHECK (BandDesign::design (s, fs).isStable());
        }
}

//==============================================================================
TEST_CASE ("Loading a factory preset sets exactly its bands and frees the rest", "[preset][manager]")
{
    Fixture f;
    // Leave something in the way: a band that must be freed, and output settings that must reset.
    setBand (f.processor, 14, FilterType::notch, 700.0f, 0.0f, 8.0f, 3, true);
    set (f.processor, Parameters::outputGain, 6.0f);
    set (f.processor, Parameters::outputInvert, 1.0f);
    GestureCounter gestures (f.processor);

    const auto entries = f.presets.getEntries();
    REQUIRE (entries.size() >= FactoryPresets::all().size());
    const auto kick = std::find_if (entries.begin(), entries.end(), [] (const auto& e) { return e.name == "Kick"; });
    REQUIRE (kick != entries.end());
    REQUIRE (f.presets.load (*kick));

    const auto bands = f.processor.getBandSettings();
    CHECK (bands[0].type == FilterType::lowCut);
    CHECK_THAT (bands[0].frequencyHz, WithinRel (35.0, 1e-3));
    CHECK (bands[1].type == FilterType::bell);
    CHECK_THAT (bands[1].frequencyHz, WithinRel (80.0, 1e-3));
    CHECK_THAT (bands[1].gainDb, WithinAbs (2.0, 0.01));
    CHECK_THAT (bands[1].q, WithinRel (1.4, 1e-3));
    for (int b = 1; b <= 16; ++b)
    {
        INFO ("band " << b);
        CHECK (f.processor.isBandInUse (b) == (b <= 4));
    }
    CHECK_THAT (value (f.processor, Parameters::outputGain), WithinAbs (0.0f, 1e-4f));
    CHECK_THAT (value (f.processor, Parameters::outputInvert), WithinAbs (0.0f, 0.0f));

    CHECK (f.presets.getCurrentName() == "Kick");
    CHECK (f.presets.isCurrentFactory());
    CHECK_FALSE (f.presets.isModified());
    CHECK (gestures.allMatched());

    // Any change marks it modified.
    set (f.processor, "band2_gain", 3.0f);
    CHECK (f.presets.isModified());
}

TEST_CASE ("Loading a preset during playback does not click", "[preset][manager]")
{
    Fixture f;
    constexpr double fs = 44100.0;
    f.processor.setPlayConfigDetails (2, 2, fs, 512);
    f.processor.prepareToPlay (fs, 512);

    juce::AudioBuffer<float> buffer (2, 16384);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 16384; ++i)
            buffer.setSample (ch, i, static_cast<float> (0.25 * std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * i / fs)));

    const auto entries = f.presets.getEntries();
    juce::MidiBuffer midi;
    for (int start = 0, next = 0; start < 16384; start += 512)
    {
        if (start % 2048 == 1024 && next < static_cast<int> (entries.size()))
            f.presets.load (entries[static_cast<size_t> (next++)]);
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, start, 512);
        f.processor.processBlock (view, midi);
    }

    double largest = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 1; i < 16384; ++i)
            largest = std::max (largest, std::abs (static_cast<double> (buffer.getSample (ch, i)) - buffer.getSample (ch, i - 1)));
    CHECK (largest < 0.2);
}

TEST_CASE ("Capture and apply round-trip the whole sound", "[preset][manager]")
{
    Fixture f;
    f.presets.apply (sample());
    const auto captured = f.presets.capture ("Again");
    CHECK (captured.hasSameSettingsAs (sample()));
    CHECK (captured.name == "Again");
}

//==============================================================================
TEST_CASE ("User presets: save, list, load, overwrite, delete", "[preset][user]")
{
    Fixture f;
    f.presets.apply (sample());

    CHECK (f.presets.saveUserPreset ("Warm Vox", false) == PresetManager::SaveResult::saved);
    const auto file = f.folder.dir.getChildFile ("Warm Vox.xml");
    CHECK (file.existsAsFile());
    CHECK (f.presets.getCurrentName() == "Warm Vox");
    CHECK_FALSE (f.presets.isCurrentFactory());
    CHECK_FALSE (f.presets.isModified());

    // Listed after the factory presets, sorted by name.
    f.presets.saveUserPreset ("Aardvark", false);
    const auto entries = f.presets.getEntries();
    REQUIRE (entries.size() == FactoryPresets::all().size() + 2);
    CHECK (entries[FactoryPresets::all().size()].name == "Aardvark");
    CHECK (entries.back().name == "Warm Vox");
    CHECK_FALSE (entries.back().isFactory);

    // Overwrite only when asked.
    set (f.processor, Parameters::outputGain, 4.0f);
    CHECK (f.presets.saveUserPreset ("Warm Vox", false) == PresetManager::SaveResult::alreadyExists);
    CHECK (f.presets.saveUserPreset ("Warm Vox", true) == PresetManager::SaveResult::saved);

    // Load it back after changing everything.
    f.presets.load (f.presets.getEntries().front());
    REQUIRE (f.presets.load (f.presets.getEntries().back()));
    CHECK_THAT (value (f.processor, Parameters::outputGain), WithinAbs (4.0f, 1e-4f));
    CHECK (f.processor.getBandSettings()[4].channel == ChannelMode::mid);

    // Delete.
    CHECK (f.presets.deleteUserPreset (f.presets.getEntries().back()));
    CHECK_FALSE (file.existsAsFile());
    CHECK (f.presets.getEntries().size() == FactoryPresets::all().size() + 1);

    // Factory presets cannot be deleted.
    CHECK_FALSE (f.presets.deleteUserPreset (f.presets.getEntries().front()));
}

TEST_CASE ("Preset names become safe file names", "[preset][user]")
{
    CHECK (PresetManager::toFileName ("Warm Vox") == "Warm Vox");
    CHECK (PresetManager::toFileName ("A/B: \"C\"?") == "AB C");
    CHECK (PresetManager::toFileName ("  ../..  ").isEmpty());
    CHECK (PresetManager::toFileName ("").isEmpty());

    Fixture f;
    CHECK (f.presets.saveUserPreset ("///", false) == PresetManager::SaveResult::invalidName);
    CHECK (f.presets.saveUserPreset ("   ", false) == PresetManager::SaveResult::invalidName);
}

TEST_CASE ("Next and previous step through factory and user presets and wrap", "[preset][user]")
{
    Fixture f;
    f.presets.saveUserPreset ("Zed", false);
    const auto entries = f.presets.getEntries();

    f.presets.load (entries.front());
    f.presets.loadPrevious();
    CHECK (f.presets.getCurrentName() == "Zed");      // wraps to the last (user) preset
    f.presets.loadNext();
    CHECK (f.presets.getCurrentName() == entries.front().name);
    f.presets.loadNext();
    CHECK (f.presets.getCurrentName() == entries[1].name);
}

TEST_CASE ("The default user folder is the macOS plugin preset folder", "[preset][user]")
{
    CHECK (PresetManager::defaultUserFolder().getFullPathName().endsWith ("Library/Audio/Presets/Catastrophic Audio/Spectral Fault"));
    CHECK (PresetManager::legacyUserFolder().getFullPathName().endsWith ("Library/Audio/Presets/CatastrophicCoder/ParametricEQ"));
}

TEST_CASE ("User presets from the old folder are copied to the new one once", "[preset][user][migration]")
{
    TempFolder folder;
    const auto legacy = folder.dir.getChildFile ("old");
    const auto target = folder.dir.getChildFile ("new");
    legacy.createDirectory();

    Preset a; a.name = "Vocal Take";
    Preset b; b.name = "Drum Bus";
    REQUIRE (a.toXml()->writeTo (legacy.getChildFile ("Vocal Take.xml")));
    REQUIRE (b.toXml()->writeTo (legacy.getChildFile ("Drum Bus.xml")));
    REQUIRE (legacy.getChildFile ("notes.txt").replaceWithText ("not a preset"));

    SECTION ("new folder missing: every .xml is copied, the old files stay")
    {
        CHECK (PresetManager::copyLegacyPresets (legacy, target) == 2);
        CHECK (target.getChildFile ("Vocal Take.xml").existsAsFile());
        CHECK (target.getChildFile ("Drum Bus.xml").existsAsFile());
        CHECK_FALSE (target.getChildFile ("notes.txt").exists());
        CHECK (legacy.getChildFile ("Vocal Take.xml").existsAsFile());

        // A second run finds presets in the new folder and does nothing.
        legacy.getChildFile ("Late.xml").replaceWithText (a.toXml()->toString());
        CHECK (PresetManager::copyLegacyPresets (legacy, target) == 0);
        CHECK_FALSE (target.getChildFile ("Late.xml").exists());
    }

    SECTION ("new folder already has presets: nothing is copied")
    {
        target.createDirectory();
        Preset c; c.name = "Mine";
        REQUIRE (c.toXml()->writeTo (target.getChildFile ("Mine.xml")));
        CHECK (PresetManager::copyLegacyPresets (legacy, target) == 0);
        CHECK_FALSE (target.getChildFile ("Vocal Take.xml").exists());
    }

    SECTION ("no old folder: nothing happens, no new folder is created")
    {
        CHECK (PresetManager::copyLegacyPresets (folder.dir.getChildFile ("missing"), target) == 0);
        CHECK_FALSE (target.exists());
    }
}

TEST_CASE ("A manager on a folder other than the default never migrates", "[preset][user][migration]")
{
    // Tests point managers at temporary folders; copying into them would hide real bugs
    // and, for the default folder, touch the real user presets.
    Fixture f;
    const auto entries = f.presets.getEntries();
    CHECK (std::none_of (entries.begin(), entries.end(), [] (const auto& e) { return ! e.isFactory; }));
    CHECK (f.folder.dir.findChildFiles (juce::File::findFiles, false, "*.xml").isEmpty());
}

TEST_CASE ("The current preset is remembered with the session", "[preset][state]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    TempFolder folder;

    juce::MemoryBlock saved;
    {
        ParametricEQAudioProcessor source;
        source.getPresetManager().setUserFolder (folder.dir);
        const auto entries = source.getPresetManager().getEntries();
        source.getPresetManager().load (entries[3]);   // Acoustic Guitar
        set (source, "band2_gain", -1.0f);             // then tweaked
        source.getStateInformation (saved);
    }

    ParametricEQAudioProcessor target;
    target.getPresetManager().setUserFolder (folder.dir);
    target.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));

    CHECK (target.getPresetManager().getCurrentName() == "Acoustic Guitar");
    CHECK (target.getPresetManager().isCurrentFactory());
    CHECK (target.getPresetManager().isModified());   // the tweak is recognised
}

//==============================================================================
TEST_CASE ("The top bar shows the preset and drives the browser", "[preset][editor]")
{
    Fixture f;
    std::unique_ptr<juce::AudioProcessorEditor> base (f.processor.createEditor());
    auto& editor = *dynamic_cast<ParametricEQAudioProcessorEditor*> (base.get());
    auto& bar = editor.getTopBar();

    // Menu: one submenu per category with its factory presets, plus Save As; Delete only for user presets.
    auto count = [] (const juce::PopupMenu& menu, int from, int to)
    {
        int n = 0;
        for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
            if (it.getItem().itemID >= from && it.getItem().itemID < to) ++n;
        return n;
    };
    auto item = [] (const juce::PopupMenu& menu, int id) -> std::optional<juce::PopupMenu::Item>
    {
        for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
            if (it.getItem().itemID == id) return it.getItem();
        return std::nullopt;
    };

    auto menu = bar.buildPresetMenu();
    CHECK (count (menu, TopBar::menuFactoryBase, TopBar::menuUserBase) == static_cast<int> (FactoryPresets::all().size()));
    REQUIRE (item (menu, TopBar::menuSaveAs).has_value());
    REQUIRE (item (menu, TopBar::menuDelete).has_value());
    CHECK_FALSE (item (menu, TopBar::menuDelete)->isEnabled);   // nothing user-owned loaded

    // Choosing a factory preset loads it and shows its name.
    bar.applyPresetMenuResult (TopBar::menuFactoryBase + 7);   // Kick
    bar.refresh();
    CHECK (f.presets.getCurrentName() == "Kick");
    CHECK (bar.getPresetButton().getButtonText() == "Kick");

    // A change shows "*".
    set (f.processor, "band2_gain", 1.0f);
    bar.refresh();
    CHECK (bar.getPresetButton().getButtonText() == "Kick *");

    // Save As creates a user preset, which then appears in the menu and can be deleted.
    bar.saveAs ("My Kick");
    bar.refresh();
    CHECK (bar.getPresetButton().getButtonText() == "My Kick");
    menu = bar.buildPresetMenu();
    CHECK (count (menu, TopBar::menuUserBase, TopBar::menuSaveAs) == 1);
    CHECK (item (menu, TopBar::menuDelete)->isEnabled);
    bar.applyPresetMenuResult (TopBar::menuDelete);
    CHECK_FALSE (f.folder.dir.getChildFile ("My Kick.xml").existsAsFile());

    // Previous / next buttons.
    REQUIRE (bar.getNextButton().onClick != nullptr);
    f.presets.load (f.presets.getEntries().front());
    bar.getNextButton().onClick();
    CHECK (f.presets.getCurrentName() == f.presets.getEntries()[1].name);
    bar.getPreviousButton().onClick();
    CHECK (f.presets.getCurrentName() == f.presets.getEntries()[0].name);
}
