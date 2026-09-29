#include "MatchSession.h"

#include "PluginProcessor.h"

MatchSession::MatchSession (ParametricEQAudioProcessor& p) : processor (p) {}
bool MatchSession::usesSidechain() const { return false; }
void MatchSession::startLearning (Learning) {}
void MatchSession::stopLearning() {}
void MatchSession::addInputSamples (const float*, int) {}
void MatchSession::addSidechainSamples (const float*, int) {}
bool MatchSession::canApply() const noexcept { return false; }
std::vector<double> MatchSession::curveDb (const std::vector<double>& f) const { return std::vector<double> (f.size(), 0.0); }
int MatchSession::freeSlots() const { return 0; }
void MatchSession::apply (ApplyMode) {}
