#pragma once

#include "dsp/BandSettings.h"

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

    /** Writes a fitted band's settings (type, frequency, gain, Q, slope, channel, dynamics off,
        enabled) as complete edits (EQ Sketch, EQ Match). The caller marks the band in use. */
    void writeFittedBand (int band, const BandSettings& settings);

private:
    juce::RangedAudioParameter* parameter (int band, const char* field) const;

    juce::AudioProcessorValueTreeState& state;
};
