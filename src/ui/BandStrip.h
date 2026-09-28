#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/** Controls for one band: on/off, type, frequency, gain, Q and slope, each
    attached to its band<n>_* parameter. Controls the band's type does not use
    are greyed out by refreshControlStates().
*/
class BandStrip final : public juce::Component
{
public:
    BandStrip (juce::AudioProcessorValueTreeState& state, int bandNumber);
    ~BandStrip() override;

    /** Greys out controls the current type does not use. Call on the message thread. */
    void refreshControlStates();

    void resized() override;
    void lookAndFeelChanged() override;

    juce::ToggleButton& getEnableButton() noexcept { return enable; }
    juce::ComboBox& getTypeBox() noexcept          { return type; }
    juce::Slider& getFrequencySlider() noexcept    { return frequency; }
    juce::Slider& getGainSlider() noexcept         { return gain; }
    juce::Slider& getQSlider() noexcept            { return q; }
    juce::ComboBox& getSlopeBox() noexcept         { return slope; }

private:
    /** Uses full menu labels when they all fit, short ones otherwise. */
    void updateMenuLabels();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    juce::AudioProcessorValueTreeState& state;
    const int band;

    juce::ToggleButton enable;
    juce::ComboBox type, slope;
    juce::Slider frequency, gain, q;
    juce::Label frequencyCaption, gainCaption, qCaption;

    std::unique_ptr<ButtonAttachment> enableAttachment;
    std::unique_ptr<ComboBoxAttachment> typeAttachment, slopeAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment, gainAttachment, qAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandStrip)
};
