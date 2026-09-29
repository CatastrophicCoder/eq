#include "SpectrumAverager.h"

void SpectrumAverager::prepare (double rate) { sampleRate = rate; }
void SpectrumAverager::reset() {}
void SpectrumAverager::addSamples (const float*, int) {}
double SpectrumAverager::getSeconds() const noexcept { return 0.0; }
std::vector<double> SpectrumAverager::levelsDb (const std::vector<double>& f) const { return std::vector<double> (f.size(), 0.0); }
void SpectrumAverager::analyseFrame() {}
