#include "NodeLayout.h"

// Not implemented yet.
std::vector<NodeLayout::Node> NodeLayout::compute (std::span<const BandSettings>, const FrequencyAxis&) { return {}; }
int NodeLayout::bandAt (const std::vector<Node>&, juce::Point<float>, float) { return 0; }
std::vector<int> NodeLayout::bandsIn (const std::vector<Node>&, juce::Rectangle<float>) { return {}; }
