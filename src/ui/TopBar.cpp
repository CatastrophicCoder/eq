#include "TopBar.h"

#include "ResponseDisplay.h"
#include "presets/FactoryPresets.h"
#include "presets/PresetManager.h"

#include <algorithm>

TopBar::TopBar (PresetManager& p) : presets (p)
{
    setName ("topBar");

    previousButton.setName ("previousPreset");
    nextButton.setName ("nextPreset");
    presetButton.setName ("preset");
    previousButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    presetButton.setTooltip ("Presets");

    previousButton.onClick = [this] { presets.loadPrevious(); refresh(); };
    nextButton.onClick = [this] { presets.loadNext(); refresh(); };
    presetButton.onClick = [this]
    {
        juce::Component::SafePointer<TopBar> safe (this);
        buildPresetMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton),
                                         [safe] (int result)
                                         {
                                             if (safe != nullptr && result != 0)
                                                 safe->applyPresetMenuResult (result);
                                         });
    };

    for (auto* b : { &previousButton, &nextButton, &presetButton })
        addAndMakeVisible (b);
    addAndMakeVisible (phaseModeControls);

    // A/B (M9a).
    aButton.setName ("slotA");
    bButton.setName ("slotB");
    copyButton.setName ("copySlot");
    aButton.setClickingTogglesState (false);
    bButton.setClickingTogglesState (false);
    aButton.setTooltip ("Setting A");
    bButton.setTooltip ("Setting B");
    copyButton.setTooltip ("Copy the active setting to the other slot");
    aButton.onClick = [this] { if (onSlotChosen != nullptr) onSlotChosen (false); };
    bButton.onClick = [this] { if (onSlotChosen != nullptr) onSlotChosen (true); };
    copyButton.onClick = [this] { if (onCopy != nullptr) onCopy(); };
    for (auto* b : { &aButton, &bButton })
    {
        // The active slot in the warm colour of the summed curve.
        b->setColour (juce::TextButton::buttonOnColourId, ResponseDisplay::sumColour().withAlpha (0.8f));
        b->setColour (juce::TextButton::textColourOnId, juce::Colour { 0xff111116 });
    }
    for (auto* b : { &aButton, &bButton, &copyButton })
        addAndMakeVisible (b);
    showActiveSlot (false);

    // Undo/redo (M9b): buttons only; Cmd-Z stays with the host.
    undoButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xb6"));   // anticlockwise arrow
    redoButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xb7"));   // clockwise arrow
    undoButton.setName ("undo");
    redoButton.setName ("redo");
    undoButton.setTooltip ("Undo the last edit");
    redoButton.setTooltip ("Redo");
    undoButton.onClick = [this] { if (onUndo != nullptr) onUndo(); };
    redoButton.onClick = [this] { if (onRedo != nullptr) onRedo(); };
    for (auto* b : { &undoButton, &redoButton })
    {
        b->setEnabled (false);
        addAndMakeVisible (b);
    }

    refresh();
}

void TopBar::refresh()
{
    const auto name = presets.getCurrentName();
    presetButton.setButtonText (name.isEmpty() ? juce::String ("No preset")
                                               : name + (presets.isModified() ? " *" : ""));
}

juce::PopupMenu TopBar::buildPresetMenu() const
{
    const auto entries = presets.getEntries();
    const auto currentName = presets.getCurrentName();
    const auto currentFactory = presets.isCurrentFactory();
    juce::PopupMenu menu;

    // Factory presets, one submenu per category; ids follow FactoryPresets::all() order.
    for (const auto& category : FactoryPresets::categories())
    {
        juce::PopupMenu sub;
        int factoryIndex = 0;
        for (const auto& e : entries)
        {
            if (! e.isFactory)
                continue;
            if (e.category == category)
                sub.addItem (menuFactoryBase + factoryIndex, e.name, true, currentFactory && e.name == currentName);
            ++factoryIndex;
        }
        menu.addSubMenu (category, sub);
    }

    // User presets.
    juce::PopupMenu user;
    int userIndex = 0;
    for (const auto& e : entries)
        if (! e.isFactory)
            user.addItem (menuUserBase + userIndex++, e.name, true, ! currentFactory && e.name == currentName);
    if (userIndex == 0)
        user.addItem (menuSaveAs + 10, "(none yet)", false);
    menu.addSeparator();
    menu.addSubMenu ("User", user);

    menu.addSeparator();
    menu.addItem (menuSaveAs, "Save As...");
    const auto canDelete = ! currentFactory && currentName.isNotEmpty()
                           && std::any_of (entries.begin(), entries.end(),
                                           [&] (const auto& e) { return ! e.isFactory && e.name == currentName; });
    menu.addItem (menuDelete, "Delete", canDelete);
    return menu;
}

void TopBar::applyPresetMenuResult (int itemId)
{
    const auto entries = presets.getEntries();
    std::vector<PresetManager::Entry> factory, user;
    for (const auto& e : entries)
        (e.isFactory ? factory : user).push_back (e);

    if (itemId >= menuFactoryBase && itemId < menuFactoryBase + static_cast<int> (factory.size()))
        presets.load (factory[static_cast<size_t> (itemId - menuFactoryBase)]);
    else if (itemId >= menuUserBase && itemId < menuUserBase + static_cast<int> (user.size()))
        presets.load (user[static_cast<size_t> (itemId - menuUserBase)]);
    else if (itemId == menuSaveAs)
        showSaveDialog();
    else if (itemId == menuDelete)
        for (const auto& e : user)
            if (e.name == presets.getCurrentName())
            {
                presets.deleteUserPreset (e);
                break;
            }

    refresh();
}

void TopBar::saveAs (const juce::String& name)
{
    const auto result = presets.saveUserPreset (name, false);

    if (result == PresetManager::SaveResult::alreadyExists)
    {
        // Ask before replacing an existing user preset.
        juce::Component::SafePointer<TopBar> safe (this);
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle ("Replace preset?")
                                          .withMessage ("A preset called \"" + name.trim() + "\" already exists.")
                                          .withButton ("Replace")
                                          .withButton ("Cancel")
                                          .withAssociatedComponent (this),
                                      [safe, name] (int button)
                                      {
                                          if (safe != nullptr && button == 1)
                                          {
                                              safe->presets.saveUserPreset (name, true);
                                              safe->refresh();
                                          }
                                      });
        return;
    }

    if (result == PresetManager::SaveResult::invalidName || result == PresetManager::SaveResult::failed)
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::WarningIcon)
                                          .withTitle ("Preset not saved")
                                          .withMessage (result == PresetManager::SaveResult::invalidName
                                                            ? "Please use a name with letters or digits."
                                                            : "The preset file could not be written.")
                                          .withButton ("OK")
                                          .withAssociatedComponent (this),
                                      nullptr);
    refresh();
}

void TopBar::showSaveDialog()
{
    auto* window = new juce::AlertWindow ("Save preset", "Name for the new preset:", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", presets.getCurrentName());
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<TopBar> safe (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
    {
        if (safe != nullptr && result == 1)
            safe->saveAs (window->getTextEditorContents ("name"));
    }), true);
}

void TopBar::resized()
{
    auto area = getLocalBounds().reduced (6, 4);
    const auto centreWidth = juce::jmin (360, area.getWidth() / 2);
    auto centre = area.withSizeKeepingCentre (centreWidth, area.getHeight());

    previousButton.setBounds (centre.removeFromLeft (28));
    nextButton.setBounds (centre.removeFromRight (28));
    presetButton.setBounds (centre.reduced (4, 0));

    // A/B left of the preset browser (M9a), between it and the plugin name.
    auto abArea = juce::Rectangle<int> (area.getX(), area.getY(), previousButton.getX() - 12 - area.getX(), area.getHeight());
    copyButton.setBounds (abArea.removeFromRight (44));
    abArea.removeFromRight (6);
    bButton.setBounds (abArea.removeFromRight (26));
    abArea.removeFromRight (2);
    aButton.setBounds (abArea.removeFromRight (26));
    abArea.removeFromRight (8);
    redoButton.setBounds (abArea.removeFromRight (26));
    abArea.removeFromRight (2);
    undoButton.setBounds (abArea.removeFromRight (26));

    // Phase mode menus on the right (M8), in the space beside the preset browser.
    const auto rightSpace = area.getRight() - nextButton.getRight() - 12;
    const auto width = juce::jmin (230, rightSpace);
    phaseModeControls.setBounds (area.removeFromRight (width));
}

void TopBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour { 0xff111116 });
    g.setColour (juce::Colour { 0x14ffffff });
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));

    g.setColour (ResponseDisplay::sumColour());
    g.setFont (juce::FontOptions (static_cast<float> (getHeight()) * 0.5f, juce::Font::bold));
    g.drawText (JucePlugin_Name, getLocalBounds().reduced (14, 0), juce::Justification::centredLeft);
}

void TopBar::showActiveSlot (bool bIsActive)
{
    aButton.setToggleState (! bIsActive, juce::dontSendNotification);
    bButton.setToggleState (bIsActive, juce::dontSendNotification);
    copyButton.setButtonText (juce::String::fromUTF8 (bIsActive ? "B\xe2\x86\x92" "A" : "A\xe2\x86\x92" "B"));
}

void TopBar::showUndoState (bool canUndo, bool canRedo)
{
    undoButton.setEnabled (canUndo);
    redoButton.setEnabled (canRedo);
}
