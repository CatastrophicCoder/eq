#include "BandDesign.h"

#include "ButterworthCascade.h"
#include "CutSlope.h"
#include "FlatTiltDesign.h"
#include "MatchedAllpassDesign.h"
#include "MatchedBandpassDesign.h"
#include "MatchedNotchDesign.h"
#include "MatchedPeakingDesign.h"
#include "MatchedShelfDesign.h"
#include "TiltShelfDesign.h"

#include <algorithm>

SectionCascade BandDesign::design (const BandSettings& settings, double sampleRate) noexcept
{
    SectionCascade cascade;

    if (! settings.isActive())
        return cascade;

    const auto f = std::min (settings.frequencyHz, maxFrequencyRatio * sampleRate);
    const auto gain = settings.gainDb;
    const auto q = settings.q;

    switch (settings.type)
    {
        case FilterType::bell:      cascade.add (MatchedPeakingDesign::design (f, gain, q, sampleRate)); break;
        case FilterType::lowShelf:  cascade.add (MatchedShelfDesign::designLow (f, gain, sampleRate)); break;
        case FilterType::highShelf: cascade.add (MatchedShelfDesign::designHigh (f, gain, sampleRate)); break;
        case FilterType::notch:     cascade.add (MatchedNotchDesign::design (f, q, sampleRate)); break;
        case FilterType::bandPass:  cascade.add (MatchedBandpassDesign::design (f, q, sampleRate)); break;
        case FilterType::allPass:   cascade.add (MatchedAllpassDesign::design (f, q, sampleRate)); break;
        case FilterType::tiltShelf: cascade.add (TiltShelfDesign::design (f, gain, sampleRate)); break;
        case FilterType::flatTilt:  cascade = FlatTiltDesign::design (f, gain, sampleRate); break;

        case FilterType::lowCut:
            cascade = ButterworthCascade::design (ButterworthCascade::Kind::lowCut, f,
                                                  CutSlope::order (settings.slopeIndex), sampleRate);
            break;

        case FilterType::highCut:
            cascade = ButterworthCascade::design (ButterworthCascade::Kind::highCut, f,
                                                  CutSlope::order (settings.slopeIndex), sampleRate);
            break;
    }

    return cascade;
}
