#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

//==============================================================================
/** Controls for one band: on/off, type, frequency, gain, Q, slope, and (M7) a
    dynamics section: switch, mode, detector, side-chain, threshold, range, ratio,
    attack, release. The band is chosen on the display (M4: tabs removed, decision
    2026-09-28); changing it re-attaches the controls to band<n>_* parameters.
*/
class BandPanel final : public juce::Component
{
public:
    explicit BandPanel (juce::AudioProcessorValueTreeState& state);
    ~BandPanel() override;

    void setBand (int bandNumber);
    int getBand() const noexcept { return band; }

    /** Greys out controls the band's type does not use. Message thread only. */
    void refreshControlStates();

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::ToggleButton& getEnableButton() noexcept     { return enable; }
    juce::ComboBox& getTypeBox() noexcept              { return type; }
    juce::Slider& getFrequencySlider() noexcept        { return frequency; }
    juce::Slider& getGainSlider() noexcept             { return gain; }
    juce::Slider& getQSlider() noexcept                { return q; }
    juce::ComboBox& getSlopeBox() noexcept             { return slope; }
    juce::ComboBox& getChannelBox() noexcept           { return channel; }

    // Dynamics (M7).
    juce::ToggleButton& getDynamicButton() noexcept    { return dynamic; }
    juce::ComboBox& getDynamicModeBox() noexcept       { return dynamicMode; }
    juce::ComboBox& getDetectorBox() noexcept          { return detector; }
    juce::ToggleButton& getSidechainButton() noexcept  { return sidechain; }
    juce::ToggleButton& getSpectralButton() noexcept   { return spectral; }   // M9g
    juce::Slider& getThresholdSlider() noexcept        { return threshold; }
    juce::Slider& getRangeSlider() noexcept            { return range; }
    juce::Slider& getRatioSlider() noexcept            { return ratio; }
    juce::Slider& getAttackSlider() noexcept           { return attack; }
    juce::Slider& getReleaseSlider() noexcept          { return release; }

private:
    void attach();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    juce::AudioProcessorValueTreeState& state;
    int band = 1;

    juce::ToggleButton enable;
    juce::ComboBox type, slope, channel;
    juce::Slider frequency, gain, q;
    juce::Label frequencyCaption, gainCaption, qCaption;

    juce::ToggleButton dynamic, sidechain, spectral;
    juce::ComboBox dynamicMode, detector;
    juce::Slider threshold, range, ratio, attack, release;
    juce::Label thresholdCaption, rangeCaption, ratioCaption, attackCaption, releaseCaption;
    int dividerX = 0;

    std::unique_ptr<ButtonAttachment> enableAttachment;
    std::unique_ptr<ComboBoxAttachment> typeAttachment, slopeAttachment, channelAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment, gainAttachment, qAttachment;

    std::unique_ptr<ButtonAttachment> dynamicAttachment, sidechainAttachment, spectralAttachment;
    std::unique_ptr<ComboBoxAttachment> dynamicModeAttachment, detectorAttachment;
    std::unique_ptr<SliderAttachment> thresholdAttachment, rangeAttachment, ratioAttachment, attackAttachment, releaseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
};
