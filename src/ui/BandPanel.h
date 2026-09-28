#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

//==============================================================================
/** Controls for one band at a time, with 16 tabs to pick the band (until M4 adds
    click-to-select on the display): on/off, type, frequency, gain, Q, slope.
    Changing the band re-attaches the controls to band<n>_* parameters.
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

    juce::TextButton& getTab (int bandNumber) noexcept { return tabs[static_cast<size_t> (bandNumber - 1)]; }
    juce::ToggleButton& getEnableButton() noexcept     { return enable; }
    juce::ComboBox& getTypeBox() noexcept              { return type; }
    juce::Slider& getFrequencySlider() noexcept        { return frequency; }
    juce::Slider& getGainSlider() noexcept             { return gain; }
    juce::Slider& getQSlider() noexcept                { return q; }
    juce::ComboBox& getSlopeBox() noexcept             { return slope; }

private:
    void attach();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    juce::AudioProcessorValueTreeState& state;
    int band = 1;

    std::array<juce::TextButton, 16> tabs;
    juce::ToggleButton enable;
    juce::ComboBox type, slope;
    juce::Slider frequency, gain, q;
    juce::Label frequencyCaption, gainCaption, qCaption;

    std::unique_ptr<ButtonAttachment> enableAttachment;
    std::unique_ptr<ComboBoxAttachment> typeAttachment, slopeAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment, gainAttachment, qAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
};
