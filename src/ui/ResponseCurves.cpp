#include "ResponseCurves.h"

// Not implemented yet.
ResponseCurves::ResponseCurves() = default;
bool ResponseCurves::update (std::span<const BandSettings>, double) { return false; }
double ResponseCurves::bandDb (int band, int point) const noexcept
{
    return curves[static_cast<size_t> (band)][static_cast<size_t> (point)];
}
