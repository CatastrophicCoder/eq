#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** The default JUCE V4 look with tighter combo boxes, so type and slope menus
    stay readable in the narrow band columns: a smaller arrow area, less label
    padding and a 12 pt font. Colours are the V4 defaults.
*/
class CompactLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    static constexpr float menuFontHeight = 12.0f;

    /** Width reserved for the arrow in a box of the given height. */
    static int arrowWidth (int boxHeight) noexcept { return juce::jlimit (10, 16, boxHeight / 2 + 2); }

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
};
