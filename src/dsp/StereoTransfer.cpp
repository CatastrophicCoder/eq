#include "StereoTransfer.h"

// Not implemented yet.
StereoTransfer::Matrix StereoTransfer::identity() noexcept { return {}; }
StereoTransfer::Matrix StereoTransfer::multiply (const Matrix&, const Matrix&) noexcept { return {}; }
StereoTransfer::Matrix StereoTransfer::forBand (ChannelMode, Complex) noexcept { return {}; }
StereoTransfer::Matrix StereoTransfer::chain (std::span<const BandSettings>, double, double, bool) { return {}; }
double StereoTransfer::powerGain (const Matrix&) noexcept { return 0.0; }
StereoTransfer::Matrix StereoTransfer::toMidSide (const Matrix&) noexcept { return {}; }
