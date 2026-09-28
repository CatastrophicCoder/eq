#include "LevelDetector.h"

// Not implemented yet.
void LevelDetector::prepare (double newSampleRate) { sampleRate = newSampleRate; }
void LevelDetector::setTimes (double, double) noexcept {}
void LevelDetector::reset() noexcept {}
double LevelDetector::process (double) noexcept { return floorDb; }
