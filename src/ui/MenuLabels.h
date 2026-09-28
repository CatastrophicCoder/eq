#pragma once

#include "dsp/CutSlope.h"
#include "dsp/FilterType.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** Full or short menu labels for the type and slope menus, whichever fits. */
namespace MenuLabels
{
    // Short forms, index order as FilterTypes::names / CutSlope::labels.
    inline constexpr const char* shortTypeNames[FilterTypes::count] { "Bell", "LoShf", "HiShf", "LoCut", "HiCut",
                                                                      "Notch", "BPass", "Tilt", "FlatT", "AllP" };
    inline constexpr const char* shortSlopeLabels[CutSlope::count] { "6", "12", "18", "24", "30", "36", "42", "48", "54",
                                                                     "60", "66", "72", "78", "84", "90", "96", "BW" };

    /** Uses the full labels if every one fits the box's text area, the short ones otherwise. */
    template <size_t N>
    void choose (juce::ComboBox& box, const char* const (&full)[N], const char* const (&brief)[N])
    {
        auto* label = dynamic_cast<juce::Label*> (box.getChildComponent (0));
        const auto font = label != nullptr ? label->getFont() : juce::Font (juce::FontOptions (12.0f));
        const auto available = label != nullptr
                                 ? static_cast<float> (label->getBorderSize().subtractedFrom (label->getLocalBounds()).getWidth())
                                 : static_cast<float> (box.getWidth());

        bool allFit = true;
        for (auto* text : full)
            allFit = allFit && juce::GlyphArrangement::getStringWidth (font, text) <= available;

        // Read the selection first: getSelectedId() returns 0 once the shown text no longer
        // matches the item text, which changeItemText below causes.
        const auto selected = box.getSelectedId();

        for (size_t i = 0; i < N; ++i)
            box.changeItemText (static_cast<int> (i) + 1, allFit ? full[i] : brief[i]);

        box.setSelectedId (selected, juce::dontSendNotification);
    }

    inline void chooseForType (juce::ComboBox& box)  { choose (box, FilterTypes::names, shortTypeNames); }
    inline void chooseForSlope (juce::ComboBox& box) { choose (box, CutSlope::labels, shortSlopeLabels); }
}
