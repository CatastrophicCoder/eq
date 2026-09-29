#pragma once

#include "BandSettings.h"

#include <vector>

//==============================================================================
/** Fits bands to a target curve (M9d EQ Sketch; reused by EQ Match).

    The target is the dB curve the fitted bands should add up to, on a set of
    frequencies with weights: inside [rangeLowHz, rangeHighHz] the wanted shape,
    outside it 0 dB (with outsideWeight) so the bands do not spill over.

    Steps: an end of the range that levels off becomes a shelf, one that drops
    steeply becomes a Butterworth cut (slope from the drop); bells are then added
    one at a time where the remaining error is largest until every slot is used
    (decision 2026-09-29); all parameters are refined together by damped
    Gauss-Newton (Levenberg-Marquardt) least squares on the plugin's own designs.
    Fitted bands are Stereo, enabled, without dynamics. Not real-time.
*/
class CurveFitter
{
public:
    static constexpr double outsideWeight = 0.5;

    struct Problem
    {
        std::vector<double> frequenciesHz, targetDb, weights;
        double rangeLowHz = 20.0, rangeHighHz = 20000.0;
        double sampleRate = 48000.0;
    };

    static std::vector<BandSettings> fit (const Problem& problem, int slots);
};
