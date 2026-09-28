#include "SpectrumAnalyzer.h"

// Not implemented yet.
SpectrumAnalyzer::SpectrumAnalyzer() { levels.fill (floorDb); }
void SpectrumAnalyzer::setFftOrder (int order) { fftOrder = order; fftSize = 1 << order; }
void SpectrumAnalyzer::addSamples (const float*, int) {}
void SpectrumAnalyzer::update (double, double) {}
void SpectrumAnalyzer::reset() {}
double SpectrumAnalyzer::displayDb (int point) const noexcept { return levels[static_cast<size_t> (point)]; }
