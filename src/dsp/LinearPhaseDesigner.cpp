#include "LinearPhaseDesigner.h"

#include "BandDesign.h"
#include "StereoTransfer.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <numbers>

namespace
{
    /** |H(e^jw)|^2 of one section from cos w and cos 2w (no complex arithmetic). */
    double sectionPower (const BiquadCoefficients& c, double cosW, double cos2W) noexcept
    {
        const auto num = c.b0 * c.b0 + c.b1 * c.b1 + c.b2 * c.b2 + 2.0 * (c.b0 * c.b1 + c.b1 * c.b2) * cosW + 2.0 * c.b0 * c.b2 * cos2W;
        const auto den = 1.0 + c.a1 * c.a1 + c.a2 * c.a2 + 2.0 * (c.a1 + c.a1 * c.a2) * cosW + 2.0 * c.a2 * cos2W;
        return std::max (0.0, num / den);   // a zero of the response (e.g. DC of a band pass) can round below 0
    }

    /** 4-term Blackman-Harris window over length + 1 points, evaluated at m (0..length); Harris 1978, table 1. */
    double blackmanHarris (int m, int length) noexcept
    {
        const auto x = 2.0 * std::numbers::pi * m / length;
        return 0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2.0 * x) - 0.01168 * std::cos (3.0 * x);
    }
}

void LinearPhaseDesigner::design (std::span<const BandSettings> bands, double sampleRate, int numTaps, Result& result)
{
    jassert (juce::isPowerOfTwo (numTaps));
    const auto fftSize = numTaps * oversampling;
    const auto bins = fftSize / 2 + 1;

    // Bands in the filter: active and not dynamic, in chain order.
    struct Entry { SectionCascade cascade; ChannelMode mode; };
    std::vector<Entry> entries;
    for (const auto& b : bands)
        if (b.isActive() && ! b.isDynamic())
            entries.push_back ({ BandDesign::design (b, sampleRate), b.channel });

    result.numTaps = numTaps;
    result.hasCrossTerms = std::any_of (entries.begin(), entries.end(), [] (const Entry& e) { return ChannelModes::isMidSide (e.mode); });

    // Zero-phase response of each matrix term on the FFT grid. With real band magnitudes every
    // entry of the chain's 2x2 matrix is real (StereoTransfer with H = |H|).
    std::array<std::vector<double>, 4> spectra;   // [0][0], [0][1], [1][0], [1][1]
    for (auto& s : spectra)
        s.resize (static_cast<size_t> (bins));

    for (int k = 0; k < bins; ++k)
    {
        const auto w = 2.0 * std::numbers::pi * k / fftSize;
        const auto cosW = std::cos (w), cos2W = std::cos (2.0 * w);
        auto m = StereoTransfer::identity();

        for (const auto& e : entries)
        {
            double power = 1.0;
            for (int i = 0; i < e.cascade.numSections; ++i)
                power *= sectionPower (e.cascade.sections[static_cast<size_t> (i)], cosW, cos2W);
            m = StereoTransfer::multiply (StereoTransfer::forBand (e.mode, { std::sqrt (power), 0.0 }), m);
        }

        spectra[0][static_cast<size_t> (k)] = m[0][0].real();
        spectra[1][static_cast<size_t> (k)] = m[0][1].real();
        spectra[2][static_cast<size_t> (k)] = m[1][0].real();
        spectra[3][static_cast<size_t> (k)] = m[1][1].real();
    }

    juce::dsp::FFT fft (juce::roundToInt (std::log2 (fftSize)));
    std::vector<float> work (static_cast<size_t> (2 * fftSize));
    const auto centre = numTaps / 2;

    auto makeTaps = [&] (const std::vector<double>& spectrum, std::vector<float>& taps)
    {
        // Real, even spectrum -> real, even impulse response h[n] = h[-n] (JUCE's inverse includes 1/N).
        std::fill (work.begin(), work.end(), 0.0f);
        for (int k = 0; k < bins; ++k)
            work[static_cast<size_t> (2 * k)] = static_cast<float> (spectrum[static_cast<size_t> (k)]);
        fft.performRealOnlyInverseTransform (work.data());

        // Centre on tap numTaps / 2 and window. Both halves come from h[j] (j >= 0), so the taps are
        // exactly symmetric; tap 0 (unpaired, window ~6e-5) is set to zero.
        taps.assign (static_cast<size_t> (numTaps), 0.0f);
        for (int j = 0; j < centre; ++j)
        {
            const auto v = static_cast<float> (work[static_cast<size_t> (j)] * blackmanHarris (centre + j, numTaps));
            taps[static_cast<size_t> (centre + j)] = v;
            if (j > 0)
                taps[static_cast<size_t> (centre - j)] = v;
        }
    };

    makeTaps (spectra[0], result.leftFromLeft);
    makeTaps (spectra[3], result.rightFromRight);

    if (result.hasCrossTerms)
    {
        makeTaps (spectra[1], result.leftFromRight);
        makeTaps (spectra[2], result.rightFromLeft);
    }
    else
    {
        result.leftFromRight.clear();
        result.rightFromLeft.clear();
    }
}
