#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/** Writes band parameters from the UI with host gestures, so automation records
    each mouse edit as one move. Message thread only.
*/
class BandParameterWriter
{
public:
    explicit BandParameterWriter (juce::AudioProcessorValueTreeState& state);

    float get (int band, const char* field) const;

    void beginGesture (int band, const char* field);
    void set (int band, const char* field, float value);   // inside a gesture
    void endGesture (int band, const char* field);

    /** One complete edit: begin, set, end. */
    void setOnce (int band, const char* field, float value);

private:
    juce::RangedAudioParameter* parameter (int band, const char* field) const;

    juce::AudioProcessorValueTreeState& state;
};
