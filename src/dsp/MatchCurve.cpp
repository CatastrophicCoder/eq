#include "MatchCurve.h"

#include <algorithm>
#include <cmath>

std::vector<double> MatchCurve::smooth (const std::vector<double>& f, const std::vector<double>& db, double octaves)
{
    std::vector<double> result (db.size());
    const auto half = 0.5 * octaves;

    for (size_t i = 0; i < f.size(); ++i)
    {
        // Power average of the points within half the width on either side (log frequency).
        double power = 0.0;
        int count = 0;
        for (size_t j = 0; j < f.size(); ++j)
            if (std::abs (std::log2 (f[j] / f[i])) <= half + 1.0e-12)
            {
                power += std::pow (10.0, db[j] / 10.0);
                ++count;
            }
        result[i] = 10.0 * std::log10 (power / count);
    }

    return result;
}

std::vector<double> MatchCurve::compute (const std::vector<double>& f, const std::vector<double>& referenceDb,
                                         const std::vector<double>& currentDb, double amount, double smoothingOctaves)
{
    const auto reference = smooth (f, referenceDb, smoothingOctaves);
    const auto current = smooth (f, currentDb, smoothingOctaves);

    std::vector<double> difference (f.size());
    double mean = 0.0;
    int count = 0;
    for (size_t i = 0; i < f.size(); ++i)
    {
        difference[i] = reference[i] - current[i];
        if (f[i] >= levelLowHz && f[i] <= levelHighHz)
        {
            mean += difference[i];
            ++count;
        }
    }
    if (count > 0)
        mean /= count;

    // Remove the overall level difference, then scale and limit.
    for (auto& d : difference)
        d = std::clamp ((d - mean) * amount, -maxDb, maxDb);

    return difference;
}
