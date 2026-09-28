#include "CompactLookAndFeel.h"

juce::Font CompactLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (menuFontHeight));
}

void CompactLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (1, 1, box.getWidth() - arrowWidth (box.getHeight()) - 1, box.getHeight() - 2);
    label.setBorderSize ({ 1, 3, 1, 1 });
    label.setFont (getComboBoxFont (box));
}

void CompactLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox& box)
{
    const juce::Rectangle<float> bounds (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

    const auto arrow = static_cast<float> (arrowWidth (height));
    const auto cx = static_cast<float> (width) - arrow / 2.0f - 1.0f;
    const auto cy = static_cast<float> (height) / 2.0f;
    const auto size = arrow * 0.3f;

    juce::Path path;
    path.startNewSubPath (cx - size, cy - size / 2.0f);
    path.lineTo (cx, cy + size / 2.0f);
    path.lineTo (cx + size, cy - size / 2.0f);

    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 0.9f : 0.2f));
    g.strokePath (path, juce::PathStrokeType (1.5f));
}
