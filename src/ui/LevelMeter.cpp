#include "LevelMeter.h"

// Not implemented yet.
void LevelMeter::addSamples (const float*, const float*, int, double) {}
void LevelMeter::update (double) {}
double LevelMeter::peakDb (int) const noexcept { return floorDb; }
double LevelMeter::rmsDb (int) const noexcept { return floorDb; }
