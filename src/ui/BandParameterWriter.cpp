#include "BandParameterWriter.h"

#include "Parameters.h"

BandParameterWriter::BandParameterWriter (juce::AudioProcessorValueTreeState& s)
    : state (s)
{
}

juce::RangedAudioParameter* BandParameterWriter::parameter (int band, const char* field) const
{
    auto* p = state.getParameter (Parameters::id (band, field));
    jassert (p != nullptr);
    return p;
}

float BandParameterWriter::get (int band, const char* field) const
{
    return state.getRawParameterValue (Parameters::id (band, field))->load();
}

void BandParameterWriter::beginGesture (int band, const char* field)
{
    if (auto* p = parameter (band, field))
        p->beginChangeGesture();
}

void BandParameterWriter::set (int band, const char* field, float value)
{
    if (auto* p = parameter (band, field))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void BandParameterWriter::endGesture (int band, const char* field)
{
    if (auto* p = parameter (band, field))
        p->endChangeGesture();
}

void BandParameterWriter::setOnce (int band, const char* field, float value)
{
    beginGesture (band, field);
    set (band, field, value);
    endGesture (band, field);
}

void BandParameterWriter::writeFittedBand (int band, const BandSettings& s)
{
    setOnce (band, "type", static_cast<float> (s.type));
    setOnce (band, "freq", static_cast<float> (s.frequencyHz));
    setOnce (band, "gain", static_cast<float> (s.gainDb));
    setOnce (band, "q", static_cast<float> (s.q));
    setOnce (band, "slope", static_cast<float> (s.slopeIndex));
    setOnce (band, "channel", static_cast<float> (s.channel));
    setOnce (band, "dyn", 0.0f);
    setOnce (band, "enabled", 1.0f);
}
