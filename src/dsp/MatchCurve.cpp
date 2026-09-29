#include "MatchCurve.h"

std::vector<double> MatchCurve::smooth (const std::vector<double>&, const std::vector<double>& db, double) { return db; }
std::vector<double> MatchCurve::compute (const std::vector<double>& f, const std::vector<double>&, const std::vector<double>&, double, double)
{
    return std::vector<double> (f.size(), 0.0);
}
