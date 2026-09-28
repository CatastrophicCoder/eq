#include "NodeDragController.h"

// Not implemented yet.
void NodeDragController::begin (std::vector<BandStart> bands, juce::Point<float> start) { starts = std::move (bands); origin = start; }
void NodeDragController::end() {}
std::vector<NodeDragController::BandValues> NodeDragController::dragTo (juce::Point<float>, bool, const FrequencyAxis&) const { return {}; }
double NodeDragController::scaleQ (double q, double) noexcept { return q; }
double NodeDragController::wheelFactor (float) noexcept { return 1.0; }
