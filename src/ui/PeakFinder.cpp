#include "PeakFinder.h"

std::vector<PeakFinder::Peak> PeakFinder::find (std::span<const double>, std::span<const double>) { return {}; }
std::optional<PeakFinder::Peak> PeakFinder::nearest (const std::vector<Peak>&, double) { return std::nullopt; }
