#include "BandParameterWriter.h"

// Not implemented yet.
BandParameterWriter::BandParameterWriter (juce::AudioProcessorValueTreeState& s) : state (s) {}
float BandParameterWriter::get (int, const char*) const { return 0.0f; }
void BandParameterWriter::beginGesture (int, const char*) {}
void BandParameterWriter::set (int, const char*, float) {}
void BandParameterWriter::endGesture (int, const char*) {}
void BandParameterWriter::setOnce (int, const char*, float) {}
juce::RangedAudioParameter* BandParameterWriter::parameter (int, const char*) const { return nullptr; }
