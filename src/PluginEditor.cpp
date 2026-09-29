#include "PluginEditor.h"

ParametricEQAudioProcessorEditor::ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      eqProcessor (p),
      topBar (p.getPresetManager()),
      display (p),
      bandPanel (p.getValueTreeState()),
      bottomBar (p.getValueTreeState(), [&p] { return p.getAutoGainOffsetDb(); })
{
    setLookAndFeel (&lookAndFeel);
    eqProcessor.setAnalyzerActive (true);   // the taps run only while an editor is open

    bottomBar.showAnalyzerSettings (p.getAnalyzerSettings());
    bottomBar.onAnalyzerSettingsChanged = [this]
    {
        eqProcessor.setAnalyzerSettings (bottomBar.getAnalyzerSettingsShown());
        display.setAnalyzerFrozen (bottomBar.getFreezeButton().getToggleState());
        display.refreshAnalyzer (0.0);
    };

    auto& phase = topBar.getPhaseModeControls();
    phase.onModeChanged = [this] (bool linear) { eqProcessor.setLinearPhase (linear); };
    phase.onLengthChanged = [this] (int index) { eqProcessor.setLinearPhaseLength (index); };

    addAndMakeVisible (topBar);
    addAndMakeVisible (display);
    addAndMakeVisible (bottomBar);
    addChildComponent (bandPanel);   // after the display: drawn on top of it; shown with a selection

    display.onSelectionChanged = [this]
    {
        const auto primary = display.getSelection().getPrimary();
        if (primary != 0 && primary != bandPanel.getBand())
            bandPanel.setBand (primary);
        bandPanel.setVisible (primary != 0);
    };

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
    setSize (defaultWidth, defaultHeight);

    refreshControls();
    startTimerHz (30);
}

ParametricEQAudioProcessorEditor::~ParametricEQAudioProcessorEditor()
{
    stopTimer();
    eqProcessor.setAnalyzerActive (false);
    setLookAndFeel (nullptr);
}

void ParametricEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour { 0xff17171d });
}

void ParametricEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    const auto h = getHeight();
    const auto w = getWidth();

    topBar.setBounds (area.removeFromTop (juce::jlimit (26, 44, h * 45 / 1000)));
    bottomBar.setBounds (area.removeFromBottom (juce::jlimit (24, 36, h * 35 / 1000)));
    display.setBounds (area);

    // Band panel: centred over the lower part of the display, above the frequency labels.
    // Wide enough for the filter and dynamics sections (M7).
    const auto panelWidth = juce::jlimit (780, 1240, w * 58 / 100);
    const auto panelHeight = juce::jlimit (118, 190, h * 19 / 100);
    const auto panelBottom = display.getBottom() - ResponseDisplay::labelStripHeight - 8;
    bandPanel.setBounds (display.getX() + (display.getWidth() - panelWidth) / 2, panelBottom - panelHeight,
                         panelWidth, panelHeight);
}

void ParametricEQAudioProcessorEditor::refreshControls()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto elapsed = lastRefreshMs > 0.0 ? juce::jlimit (0.0, 0.2, (now - lastRefreshMs) / 1000.0) : 0.0;
    lastRefreshMs = now;

    // Settings may change from outside (a loaded session): keep the menus in step.
    if (const auto stored = eqProcessor.getAnalyzerSettings(); ! (stored.mode == bottomBar.getAnalyzerSettingsShown().mode
            && stored.resolution == bottomBar.getAnalyzerSettingsShown().resolution
            && stored.speed == bottomBar.getAnalyzerSettingsShown().speed
            && stored.range == bottomBar.getAnalyzerSettingsShown().range))
        bottomBar.showAnalyzerSettings (stored);

    display.refresh();
    display.refreshAnalyzer (elapsed);
    topBar.refresh();
    topBar.getPhaseModeControls().show (eqProcessor.isLinearPhase(), eqProcessor.getLinearPhaseLength(),
                                        eqProcessor.getSampleRate() > 0.0 ? eqProcessor.getSampleRate() : 48000.0);
    bandPanel.refreshControlStates();
    bottomBar.refresh();
}
