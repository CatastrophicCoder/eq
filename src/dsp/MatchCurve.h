#pragma once

#include <vector>

//==============================================================================
/** The EQ curve that turns a "current" spectrum into a "reference" one (M9e).

    Both spectra (dB on the same frequencies) are smoothed over the given fraction
    of an octave (power average of the points within half the width either side),
    subtracted (reference - current), the mean difference between levelLowHz and
    levelHighHz is removed (a louder reference is not a boost), the result is
    scaled by amount (0-1) and limited to +-maxDb.
*/
namespace MatchCurve
{
    inline constexpr double levelLowHz = 100.0;
    inline constexpr double levelHighHz = 10000.0;
    inline constexpr double maxDb = 24.0;

    std::vector<double> smooth (const std::vector<double>& frequenciesHz, const std::vector<double>& db, double octaves);

    std::vector<double> compute (const std::vector<double>& frequenciesHz, const std::vector<double>& referenceDb,
                                 const std::vector<double>& currentDb, double amount, double smoothingOctaves);
}
