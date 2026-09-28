#include "FrequencyAxis.h"

// Not implemented yet.
FrequencyAxis::FrequencyAxis (juce::Rectangle<float> plotArea, double rangeDb) : area (plotArea), range (rangeDb) {}
float FrequencyAxis::xForFrequency (double) const noexcept { return 0.0f; }
double FrequencyAxis::frequencyForX (float) const noexcept { return 0.0; }
float FrequencyAxis::yForDb (double) const noexcept { return 0.0f; }
double FrequencyAxis::dbForY (float) const noexcept { return 0.0; }
std::vector<double> FrequencyAxis::gridFrequencies() { return {}; }
std::vector<double> FrequencyAxis::labelledFrequencies() { return {}; }
juce::String FrequencyAxis::frequencyLabel (double) { return {}; }
std::vector<double> FrequencyAxis::gridDecibels() const { return {}; }
double FrequencyAxis::gridStepDb (double) noexcept { return 0.0; }
bool FrequencyAxis::isValidRange (double) noexcept { return false; }
