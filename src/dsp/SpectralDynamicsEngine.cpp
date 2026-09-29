#include "SpectralDynamicsEngine.h"

#include "DynamicGainLaw.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

void SpectralDynamicsEngine::prepare (double rate)
{
    sampleRate = rate;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    // Square-root periodic Hann: analysis x synthesis = Hann, which sums to 2 at 75 % overlap.
    window.resize (static_cast<size_t> (fftSize));
    double windowSum = 0.0;
    for (int n = 0; n < fftSize; ++n)
    {
        window[static_cast<size_t> (n)] = static_cast<float> (std::sqrt (0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * n / fftSize)));
        windowSum += window[static_cast<size_t> (n)];
    }

    // A sine of amplitude A peaks at |X| = A (N / 2) (mean window): its slice reads A.
    calibration = 1.0 / (0.5 * windowSum);

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto c = static_cast<size_t> (ch);
        input[c].assign (static_cast<size_t> (fftSize), 0.0f);
        sidechainInput[c].assign (static_cast<size_t> (fftSize), 0.0f);
        overlap[c].assign (static_cast<size_t> (fftSize), 0.0f);
        output[c].assign (static_cast<size_t> (hop), 0.0f);
        analysis[c].assign (static_cast<size_t> (2 * fftSize), 0.0f);
        sidechainAnalysis[c].assign (static_cast<size_t> (2 * fftSize), 0.0f);
        spectrum[c].assign (static_cast<size_t> (2 * fftSize), 0.0f);
    }
    work.assign (static_cast<size_t> (2 * fftSize), 0.0f);
    envelope.assign (static_cast<size_t> (numBands * 2 * bins), 0.0);
    weights.assign (static_cast<size_t> (numBands * bins), 0.0);
    weightsValid.fill (false);
    reset();
}

void SpectralDynamicsEngine::reset() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
        for (auto* v : { &input[static_cast<size_t> (ch)], &sidechainInput[static_cast<size_t> (ch)],
                         &overlap[static_cast<size_t> (ch)], &output[static_cast<size_t> (ch)] })
            std::fill (v->begin(), v->end(), 0.0f);
    std::fill (envelope.begin(), envelope.end(), 0.0);
    for (auto& band : published)
        for (auto& g : band)
            g.store (0.0f, std::memory_order_relaxed);
    publishedNonZero.fill (false);
    position = 0;
}

void SpectralDynamicsEngine::setBands (const std::array<BandSettings, numBands>& newBands) noexcept
{
    bands = newBands;
}

double SpectralDynamicsEngine::regionWeight (const BandSettings& band, double f) noexcept
{
    // Power responses of the detector regions of M7 (bell: band pass at f0 / Q; shelves: 2nd-order
    // Butterworth lowpass / highpass at the corner), so the effect falls to half at their -3 dB points.
    const auto f0 = std::max (1.0, band.frequencyHz);
    if (band.type == FilterType::lowShelf)
        return 1.0 / (1.0 + std::pow (f / f0, 4.0));
    if (f <= 0.0)
        return 0.0;
    if (band.type == FilterType::highShelf)
        return 1.0 / (1.0 + std::pow (f0 / f, 4.0));

    const auto r = f / f0 - f0 / f;
    return 1.0 / (1.0 + band.q * band.q * r * r);
}

bool SpectralDynamicsEngine::anySpectral (const std::array<BandSettings, numBands>& b) noexcept
{
    return std::any_of (b.begin(), b.end(), [] (const BandSettings& s) { return s.isSpectral(); });
}

float SpectralDynamicsEngine::getSliceGainDb (int band, int bin) const noexcept
{
    if (band < 0 || band >= numBands || bin < 0 || bin >= bins)
        return 0.0f;
    return published[static_cast<size_t> (band)][static_cast<size_t> (bin)].load (std::memory_order_relaxed);
}

void SpectralDynamicsEngine::process (juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain) noexcept
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = std::min (2, buffer.getNumChannels());
    if (channels == 0 || fft == nullptr)
        return;

    useSidechain = sidechain != nullptr && sidechain->getNumChannels() > 0 && sidechain->getNumSamples() >= numSamples;

    for (int start = 0; start < numSamples;)
    {
        const auto n = std::min (hop - position, numSamples - start);

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto c = static_cast<size_t> (ch);
            const auto* in = buffer.getReadPointer (std::min (ch, channels - 1), start);
            std::copy_n (in, n, input[c].data() + (fftSize - hop + position));

            if (useSidechain)
                std::copy_n (sidechain->getReadPointer (std::min (ch, sidechain->getNumChannels() - 1), start), n,
                             sidechainInput[c].data() + (fftSize - hop + position));

            if (ch < channels)
                std::copy_n (output[c].data() + position, n, buffer.getWritePointer (ch, start));
        }

        position += n;
        start += n;

        if (position == hop)
        {
            processFrame();
            position = 0;
        }
    }
}

void SpectralDynamicsEngine::updateWeights (size_t b) noexcept
{
    const auto& s = bands[b];
    const auto& old = weightsFor[b];
    if (weightsValid[b] && s.type == old.type && juce::exactlyEqual (s.frequencyHz, old.frequencyHz) && juce::exactlyEqual (s.q, old.q))
        return;

    for (int k = 0; k < bins; ++k)
        weights[b * bins + static_cast<size_t> (k)] = regionWeight (s, k * sampleRate / fftSize);
    weightsFor[b] = s;
    weightsValid[b] = true;
}

void SpectralDynamicsEngine::processFrame() noexcept
{
    auto transform = [this] (const std::vector<float>& time, std::vector<float>& freq)
    {
        for (int n = 0; n < fftSize; ++n)
            work[static_cast<size_t> (n)] = time[static_cast<size_t> (n)] * window[static_cast<size_t> (n)];
        std::fill (work.begin() + fftSize, work.end(), 0.0f);
        fft->performRealOnlyForwardTransform (work.data(), true);
        std::copy_n (work.data(), 2 * bins, freq.data());
    };

    for (size_t c = 0; c < 2; ++c)
    {
        transform (input[c], analysis[c]);
        std::copy_n (analysis[c].data(), 2 * bins, spectrum[c].data());
    }

    const auto sidechainBands = useSidechain && std::any_of (bands.begin(), bands.end(),
                                                             [] (const BandSettings& s) { return s.isSpectral() && s.dynamics.sidechain; });
    if (sidechainBands)
        for (size_t c = 0; c < 2; ++c)
            transform (sidechainInput[c], sidechainAnalysis[c]);

    for (size_t b = 0; b < static_cast<size_t> (numBands); ++b)
    {
        const auto& s = bands[b];
        if (! s.isSpectral())
        {
            if (publishedNonZero[b])
            {
                for (auto& g : published[b])
                    g.store (0.0f, std::memory_order_relaxed);
                publishedNonZero[b] = false;
            }
            continue;
        }

        updateWeights (b);
        const auto& d = s.dynamics;
        const auto attack = std::exp (-hop / (std::max (0.01, d.attackMs) * 0.001 * sampleRate));
        const auto release = std::exp (-hop / (std::max (0.01, d.releaseMs) * 0.001 * sampleRate));

        // Below this level no slice can change (Ratio's knee starts 3 dB under the threshold).
        const auto quietest = std::pow (10.0, (d.thresholdDb - DynamicGainLaw::ratioKneeDb * 0.5) / 20.0);

        // Detection source: the side-chain for side-chain bands (when connected), else this frame's input.
        const auto& source = d.sidechain && useSidechain ? sidechainAnalysis : analysis;
        const auto mode = s.channel;
        const auto detectors = mode == ChannelMode::stereo ? 2 : 1;

        for (int k = 0; k < bins; ++k)
        {
            const auto w = weights[b * bins + static_cast<size_t> (k)];
            const auto re = static_cast<size_t> (2 * k), im = re + 1;
            double change[2] {};

            for (int det = 0; det < detectors; ++det)
            {
                double xr = 0.0, xi = 0.0;
                switch (mode)
                {
                    case ChannelMode::stereo: xr = source[static_cast<size_t> (det)][re]; xi = source[static_cast<size_t> (det)][im]; break;
                    case ChannelMode::left:   xr = source[0][re]; xi = source[0][im]; break;
                    case ChannelMode::right:  xr = source[1][re]; xi = source[1][im]; break;
                    case ChannelMode::mid:    xr = 0.5 * (source[0][re] + source[1][re]); xi = 0.5 * (source[0][im] + source[1][im]); break;
                    case ChannelMode::side:   xr = 0.5 * (source[0][re] - source[1][re]); xi = 0.5 * (source[0][im] - source[1][im]); break;
                }

                // Classic follower on the linear level (decision 2026-09-29), per slice.
                const auto level = std::sqrt (xr * xr + xi * xi) * calibration;
                auto& env = envelope[(b * 2 + static_cast<size_t> (det)) * bins + static_cast<size_t> (k)];
                env = level + (level > env ? attack : release) * (env - level);

                if (w > 1.0e-4 && env > quietest)
                    change[det] = w * DynamicGainLaw::gainChangeDb (d.mode, 20.0 * std::log10 (env), d.thresholdDb, d.rangeDb, d.ratio);
            }

            if (detectors == 1)
                change[1] = change[0];

            published[b][static_cast<size_t> (k)].store (static_cast<float> (std::abs (change[1]) > std::abs (change[0]) ? change[1] : change[0]),
                                                           std::memory_order_relaxed);
            if (juce::exactlyEqual (change[0], 0.0) && juce::exactlyEqual (change[1], 0.0))
                continue;

            const auto g0 = static_cast<float> (std::pow (10.0, change[0] / 20.0));
            const auto g1 = static_cast<float> (std::pow (10.0, change[1] / 20.0));
            auto& l = spectrum[0];
            auto& r = spectrum[1];

            switch (mode)
            {
                case ChannelMode::stereo: l[re] *= g0; l[im] *= g0; r[re] *= g1; r[im] *= g1; break;
                case ChannelMode::left:   l[re] *= g0; l[im] *= g0; break;
                case ChannelMode::right:  r[re] *= g0; r[im] *= g0; break;
                case ChannelMode::mid:
                case ChannelMode::side:
                {
                    // Around M = (L + R) / 2, S = (L - R) / 2: scale one of them, decode L = M + S, R = M - S.
                    for (auto i : { re, im })
                    {
                        auto m = 0.5f * (l[i] + r[i]);
                        auto sd = 0.5f * (l[i] - r[i]);
                        (mode == ChannelMode::mid ? m : sd) *= g0;
                        l[i] = m + sd;
                        r[i] = m - sd;
                    }
                    break;
                }
            }
        }
        publishedNonZero[b] = true;
    }

    // Resynthesis: inverse FFT, synthesis window, overlap-add; the oldest hop samples are complete.
    for (size_t c = 0; c < 2; ++c)
    {
        std::fill (work.begin(), work.end(), 0.0f);
        std::copy_n (spectrum[c].data(), 2 * bins, work.data());
        fft->performRealOnlyInverseTransform (work.data());

        auto& acc = overlap[c];
        for (int n = 0; n < fftSize; ++n)
            acc[static_cast<size_t> (n)] += 0.5f * work[static_cast<size_t> (n)] * window[static_cast<size_t> (n)];

        std::copy_n (acc.data(), hop, output[c].data());
        std::memmove (acc.data(), acc.data() + hop, static_cast<size_t> (fftSize - hop) * sizeof (float));
        std::fill (acc.begin() + (fftSize - hop), acc.end(), 0.0f);

        std::memmove (input[c].data(), input[c].data() + hop, static_cast<size_t> (fftSize - hop) * sizeof (float));
        std::memmove (sidechainInput[c].data(), sidechainInput[c].data() + hop, static_cast<size_t> (fftSize - hop) * sizeof (float));
    }
}
