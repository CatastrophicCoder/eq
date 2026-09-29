#include "MatchWindow.h"

MatchWindow::MatchWindow (juce::Component& content, juce::Component*)
    : juce::DocumentWindow ("EQ Match", juce::Colour { 0xff17171d }, juce::DocumentWindow::closeButton)
{
    setContentNonOwned (&content, true);
}

void MatchWindow::closeButtonPressed()
{
    if (onClose != nullptr)
        onClose();
}
