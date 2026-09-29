#include "MatchWindow.h"

MatchWindow::MatchWindow (juce::Component& content, juce::Component* near)
    : juce::DocumentWindow ("EQ Match", juce::Colour { 0xff17171d }, juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    setContentNonOwned (&content, true);
    setResizable (false, false);
    setAlwaysOnTop (true);   // stays in front of the host's plugin window
    if (near != nullptr)
        centreAroundComponent (near, getWidth(), getHeight());
    setVisible (true);
}

void MatchWindow::closeButtonPressed()
{
    if (onClose != nullptr)
        onClose();
}
