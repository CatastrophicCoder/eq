#include "AnalyzerFifo.h"

// Not implemented yet.
void AnalyzerFifo::prepare (int, int) {}
void AnalyzerFifo::reset() noexcept {}
int AnalyzerFifo::push (const float* const*, int, int) noexcept { return 0; }
int AnalyzerFifo::pull (juce::AudioBuffer<float>&) noexcept { return 0; }
int AnalyzerFifo::getNumReady() const noexcept { return 0; }
int AnalyzerFifo::getCapacity() const noexcept { return 0; }
