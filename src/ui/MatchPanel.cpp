#include "MatchPanel.h"

#include "MatchSession.h"

MatchPanel::MatchPanel (MatchSession& s) : session (s)
{
    setName ("matchPanel");
    setSize (380, 210);

    learnReference.setName ("learnReference");
    learnCurrent.setName ("learnCurrent");
    applyButton.setName ("apply");

    // With a side-chain one button learns both; without, reference and current are separate passes.
    learnReference.onClick = [this]
    {
        using L = MatchSession::Learning;
        const auto running = session.getLearning();
        if (running == L::reference || running == L::both)
            session.stopLearning();
        else
            session.startLearning (session.usesSidechain() ? L::both : L::reference);
        refresh();
    };
    learnCurrent.onClick = [this]
    {
        using L = MatchSession::Learning;
        if (session.getLearning() == L::current)
            session.stopLearning();
        else
            session.startLearning (L::current);
        refresh();
    };
    applyButton.onClick = [this] { showApplyChoice(); };

    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    amount.setRange (0.0, 100.0, 1.0);
    amount.setTextValueSuffix (" %");
    amount.setValue (100.0, juce::dontSendNotification);
    amount.onValueChange = [this] { session.setAmount (amount.getValue() / 100.0); };
    amount.setName ("amount");

    smoothing.setSliderStyle (juce::Slider::LinearHorizontal);
    smoothing.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    smoothing.setNormalisableRange ({ 1.0 / 12.0, 1.0, [] (double lo, double hi, double t) { return lo * std::pow (hi / lo, t); },
                                                       [] (double lo, double hi, double v) { return std::log (v / lo) / std::log (hi / lo); } });
    smoothing.textFromValueFunction = [] (double v) { return "1/" + juce::String (juce::roundToInt (1.0 / v)) + " oct"; };
    smoothing.setValue (1.0 / 3.0, juce::dontSendNotification);
    smoothing.onValueChange = [this] { session.setSmoothingOctaves (smoothing.getValue()); };
    smoothing.setName ("smoothing");

    amountCaption.setText ("Amount", juce::dontSendNotification);
    smoothingCaption.setText ("Smoothing", juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &learnReference, &learnCurrent, &referenceStatus, &currentStatus,
                                                              &amountCaption, &amount, &smoothingCaption, &smoothing, &applyButton })
        addAndMakeVisible (c);

    refresh();
}

void MatchPanel::refresh()
{
    // Settings may come from a loaded project.
    amount.setValue (session.getAmount() * 100.0, juce::dontSendNotification);
    smoothing.setValue (session.getSmoothingOctaves(), juce::dontSendNotification);

    using L = MatchSession::Learning;
    const auto running = session.getLearning();
    const auto sidechain = session.usesSidechain();

    learnReference.setButtonText (running == L::reference || running == L::both ? "Stop"
                                  : sidechain ? "Learn (side-chain)" : "Learn reference");
    learnCurrent.setButtonText (running == L::current ? "Stop" : "Learn current");
    learnCurrent.setVisible (! sidechain);

    auto seconds = [] (double s) { return juce::String (s, 1) + " s"; };
    referenceStatus.setText ("Reference: " + seconds (session.getReferenceSeconds()) + (sidechain ? " (side-chain)" : ""), juce::dontSendNotification);
    currentStatus.setText ("Current: " + seconds (session.getCurrentSeconds()), juce::dontSendNotification);
    applyButton.setEnabled (session.canApply() && running == L::none);
}

void MatchPanel::showApplyChoice()
{
    const auto free = session.freeSlots();
    auto* window = new juce::AlertWindow ("Apply EQ Match", "Fit the match into bands:", juce::MessageBoxIconType::NoIcon, this);
    window->addButton ("Replace all bands", 1);
    if (free > 0)
        window->addButton ("Keep existing (" + juce::String (free) + " free)", 2);
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<MatchPanel> safe (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        safe->session.apply (result == 1 ? MatchSession::ApplyMode::replaceAll : MatchSession::ApplyMode::keepExisting);
        safe->refresh();
    }), true);
}

void MatchPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour { 0xff17171d });
}

void MatchPanel::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto row = [&] (int h) { auto r = area.removeFromTop (h); area.removeFromTop (8); return r; };

    auto buttons = row (26);
    learnReference.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
    buttons.removeFromLeft (8);
    learnCurrent.setBounds (buttons);

    auto status = row (18);
    referenceStatus.setBounds (status.removeFromLeft (status.getWidth() / 2));
    currentStatus.setBounds (status);

    auto amountRow = row (24);
    amountCaption.setBounds (amountRow.removeFromLeft (80));
    amount.setBounds (amountRow);

    auto smoothingRow = row (24);
    smoothingCaption.setBounds (smoothingRow.removeFromLeft (80));
    smoothing.setBounds (smoothingRow);

    applyButton.setBounds (row (28).withSizeKeepingCentre (140, 28));
}
