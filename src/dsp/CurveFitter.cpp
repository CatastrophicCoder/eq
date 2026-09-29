#include "CurveFitter.h"

#include "BandDesign.h"

#include <algorithm>
#include <cmath>

namespace
{
    struct Fitted
    {
        BandSettings settings;
        bool fitGain = true, fitQ = true;
        std::vector<double> db;   // response at the problem's frequencies
    };

    struct Context
    {
        const CurveFitter::Problem& p;
        size_t n;
        double maxHz;

        void evaluate (Fitted& b) const
        {
            const auto design = BandDesign::design (b.settings, p.sampleRate);
            b.db.resize (n);
            for (size_t i = 0; i < n; ++i)
                b.db[i] = design.magnitudeDb (std::min (p.frequenciesHz[i], 0.49 * p.sampleRate), p.sampleRate);
        }

        std::vector<double> model (const std::vector<Fitted>& bands) const
        {
            std::vector<double> m (n, 0.0);
            for (const auto& b : bands)
                for (size_t i = 0; i < n; ++i)
                    m[i] += b.db[i];
            return m;
        }

        double cost (const std::vector<double>& m) const
        {
            double c = 0.0;
            for (size_t i = 0; i < n; ++i)
            {
                const auto e = p.weights[i] * (m[i] - p.targetDb[i]);
                c += e * e;
            }
            return c;
        }
    };

    // Parameters of one band as a vector: log2 f, then gain and log2 Q where fitted.
    std::vector<double> parametersOf (const Fitted& b)
    {
        std::vector<double> v { std::log2 (b.settings.frequencyHz) };
        if (b.fitGain) v.push_back (b.settings.gainDb);
        if (b.fitQ) v.push_back (std::log2 (b.settings.q));
        return v;
    }

    void setParameters (Fitted& b, const double* v, double maxHz)
    {
        size_t k = 0;
        b.settings.frequencyHz = std::clamp (std::exp2 (v[k++]), 20.0, maxHz);
        if (b.fitGain) b.settings.gainDb = std::clamp (v[k++], -30.0, 30.0);
        if (b.fitQ) b.settings.q = std::clamp (std::exp2 (v[k++]), 0.1, 18.0);
    }

    /** Solves A x = b (m x m, row-major) by Gaussian elimination with partial pivoting. */
    bool solve (std::vector<double> a, std::vector<double> b, std::vector<double>& x)
    {
        const auto m = b.size();
        for (size_t col = 0; col < m; ++col)
        {
            auto pivot = col;
            for (size_t r = col + 1; r < m; ++r)
                if (std::abs (a[r * m + col]) > std::abs (a[pivot * m + col]))
                    pivot = r;
            if (std::abs (a[pivot * m + col]) < 1.0e-12)
                return false;
            if (pivot != col)
            {
                for (size_t c = 0; c < m; ++c)
                    std::swap (a[col * m + c], a[pivot * m + c]);
                std::swap (b[col], b[pivot]);
            }
            for (size_t r = col + 1; r < m; ++r)
            {
                const auto factor = a[r * m + col] / a[col * m + col];
                for (size_t c = col; c < m; ++c)
                    a[r * m + c] -= factor * a[col * m + c];
                b[r] -= factor * b[col];
            }
        }
        x.assign (m, 0.0);
        for (size_t i = m; i-- > 0;)
        {
            auto sum = b[i];
            for (size_t c = i + 1; c < m; ++c)
                sum -= a[i * m + c] * x[c];
            x[i] = sum / a[i * m + i];
        }
        return true;
    }

    /** Levenberg-Marquardt over all bands' parameters (numerical Jacobian, one band recomputed per parameter). */
    void refine (const Context& ctx, std::vector<Fitted>& bands, int iterations)
    {
        double lambda = 1.0e-2;
        auto m = ctx.model (bands);
        auto cost = ctx.cost (m);

        for (int iteration = 0; iteration < iterations; ++iteration)
        {
            // Parameter layout.
            std::vector<std::pair<size_t, size_t>> owners;   // (band, index within band)
            std::vector<double> params;
            for (size_t b = 0; b < bands.size(); ++b)
            {
                const auto v = parametersOf (bands[b]);
                for (size_t k = 0; k < v.size(); ++k)
                {
                    owners.push_back ({ b, k });
                    params.push_back (v[k]);
                }
            }

            const auto count = params.size();
            std::vector<double> jacobian (count * ctx.n);   // [param][point], weighted

            for (size_t j = 0; j < count; ++j)
            {
                auto& band = bands[owners[j].first];
                auto v = parametersOf (band);
                const auto h = owners[j].second == 1 && band.fitGain ? 0.01 : 0.002;
                v[owners[j].second] += h;

                Fitted probe = band;
                setParameters (probe, v.data(), ctx.maxHz);
                ctx.evaluate (probe);
                for (size_t i = 0; i < ctx.n; ++i)
                    jacobian[j * ctx.n + i] = ctx.p.weights[i] * (probe.db[i] - band.db[i]) / h;
            }

            std::vector<double> jtj (count * count, 0.0), jte (count, 0.0);
            for (size_t a = 0; a < count; ++a)
            {
                for (size_t i = 0; i < ctx.n; ++i)
                    jte[a] += jacobian[a * ctx.n + i] * ctx.p.weights[i] * (m[i] - ctx.p.targetDb[i]);
                for (size_t b = a; b < count; ++b)
                {
                    double sum = 0.0;
                    for (size_t i = 0; i < ctx.n; ++i)
                        sum += jacobian[a * ctx.n + i] * jacobian[b * ctx.n + i];
                    jtj[a * count + b] = jtj[b * count + a] = sum;
                }
            }

            auto improved = false;
            for (int attempt = 0; attempt < 6 && ! improved; ++attempt)
            {
                auto damped = jtj;
                std::vector<double> rhs (count);
                for (size_t a = 0; a < count; ++a)
                {
                    damped[a * count + a] += lambda * (jtj[a * count + a] + 1.0e-9);
                    rhs[a] = -jte[a];
                }

                std::vector<double> step;
                if (! solve (damped, rhs, step))
                {
                    lambda *= 10.0;
                    continue;
                }

                auto trial = bands;
                for (size_t b = 0; b < trial.size(); ++b)
                {
                    auto v = parametersOf (trial[b]);
                    for (size_t j = 0; j < count; ++j)
                        if (owners[j].first == b)
                            v[owners[j].second] += step[j];
                    setParameters (trial[b], v.data(), ctx.maxHz);
                    ctx.evaluate (trial[b]);
                }

                const auto trialModel = ctx.model (trial);
                const auto trialCost = ctx.cost (trialModel);
                if (trialCost < cost)
                {
                    bands = std::move (trial);
                    m = trialModel;
                    cost = trialCost;
                    lambda = std::max (1.0e-6, lambda / 3.0);
                    improved = true;
                }
                else
                {
                    lambda *= 4.0;
                }
            }

            if (! improved)
                break;
        }
    }
}

std::vector<BandSettings> CurveFitter::fit (const Problem& problem, int slots)
{
    const Context ctx { problem, problem.frequenciesHz.size(), std::min (20000.0, 0.45 * problem.sampleRate) };
    std::vector<Fitted> bands;
    if (slots <= 0 || ctx.n < 4)
        return {};

    auto inside = [&] (size_t i) { return problem.frequenciesHz[i] >= problem.rangeLowHz && problem.frequenciesHz[i] <= problem.rangeHighHz; };
    std::vector<size_t> in;
    for (size_t i = 0; i < ctx.n; ++i)
        if (inside (i))
            in.push_back (i);
    if (in.size() < 4)
        return {};

    auto make = [] (FilterType type, double f, double gain, double q, int slope = 3)
    {
        BandSettings s;
        s.type = type; s.frequencyHz = f; s.gainDb = gain; s.q = q; s.slopeIndex = slope;
        s.enabled = true; s.inUse = true; s.channel = ChannelMode::stereo;
        return s;
    };

    // 1. The ends of the range: a level stretch becomes a shelf, a steep drop a cut.
    auto analyseEnd = [&] (bool low)
    {
        const auto endIndex = low ? in.front() : in.back();
        const auto endOctave = std::log2 (problem.frequenciesHz[endIndex]);
        const auto innerOctave = endOctave + (low ? 1.0 / 3.0 : -1.0 / 3.0);

        size_t inner = endIndex;
        for (auto i : in)
            if (std::abs (std::log2 (problem.frequenciesHz[i]) - innerOctave) < std::abs (std::log2 (problem.frequenciesHz[inner]) - innerOctave))
                inner = i;

        const auto endDb = problem.targetDb[endIndex];
        const auto dropPerOctave = (problem.targetDb[inner] - endDb) * 3.0;   // positive: falls towards the end

        if (endDb < -12.0 && dropPerOctave >= 9.0 && static_cast<int> (bands.size()) < slots)
        {
            // Cut: cutoff where the target comes back to -3 dB, order from the drop.
            double cutoff = problem.frequenciesHz[inner];
            const auto count = static_cast<int> (in.size());
            for (int k = 0; k < count; ++k)
            {
                const auto i = in[static_cast<size_t> (low ? k : count - 1 - k)];
                if (problem.targetDb[i] >= -3.0)
                {
                    cutoff = problem.frequenciesHz[i];
                    break;
                }
            }
            const auto order = std::clamp (juce::roundToInt (dropPerOctave / 6.0), 1, 16);
            Fitted f { make (low ? FilterType::lowCut : FilterType::highCut, cutoff, 0.0, 0.71, order - 1), false, false, {} };
            ctx.evaluate (f);
            bands.push_back (f);
        }
        else if (std::abs (endDb) >= 1.5 && std::abs (dropPerOctave) < 3.0 && static_cast<int> (bands.size()) < slots)
        {
            // Shelf: the plateau's gain, corner where the target has fallen to half of it (in dB).
            double corner = problem.frequenciesHz[inner];
            const auto count = static_cast<int> (in.size());
            for (int k = 0; k < count; ++k)
            {
                const auto i = in[static_cast<size_t> (low ? k : count - 1 - k)];
                if (std::abs (problem.targetDb[i]) < std::abs (endDb) * 0.5)
                {
                    corner = problem.frequenciesHz[i];
                    break;
                }
            }
            Fitted f { make (low ? FilterType::lowShelf : FilterType::highShelf, corner, endDb, 0.71), true, false, {} };
            ctx.evaluate (f);
            bands.push_back (f);
        }
    };

    // Fitted twice when the ends suggested shelves or cuts: with them and with bells only; the
    // closer fit wins (a gentle bell skirt can look like a plateau at the range's end).
    auto addBells = [&] (std::vector<Fitted> start)
    {
        auto fitted = std::move (start);
        if (! fitted.empty())
            refine (ctx, fitted, 10);

        while (static_cast<int> (fitted.size()) < slots)
        {
            const auto m = ctx.model (fitted);
            size_t peak = in.front();
            for (auto i : in)
                if (problem.weights[i] * std::abs (problem.targetDb[i] - m[i]) > problem.weights[peak] * std::abs (problem.targetDb[peak] - m[peak]))
                    peak = i;

            const auto gain = problem.targetDb[peak] - m[peak];

            // Width where the remaining error falls to half: Q = f0 / width.
            auto halfPoint = [&] (int step)
            {
                auto i = static_cast<int> (peak);
                while (i + step >= 0 && i + step < static_cast<int> (ctx.n))
                {
                    i += step;
                    const auto r = problem.targetDb[static_cast<size_t> (i)] - m[static_cast<size_t> (i)];
                    if (std::abs (r) < std::abs (gain) * 0.5 || r * gain < 0.0)
                        break;
                }
                return problem.frequenciesHz[static_cast<size_t> (i)];
            };

            const auto f0 = problem.frequenciesHz[peak];
            const auto width = std::max (halfPoint (1) - halfPoint (-1), f0 * 0.05);
            Fitted bell { make (FilterType::bell, f0, gain, std::clamp (f0 / width, 0.3, 10.0)), true, true, {} };
            ctx.evaluate (bell);
            fitted.push_back (bell);
            refine (ctx, fitted, 8);
        }

        refine (ctx, fitted, 40);
        return fitted;
    };

    analyseEnd (true);
    analyseEnd (false);

    bands = addBells (bands);
    if (bands.size() > 0 && std::any_of (bands.begin(), bands.end(), [] (const Fitted& b) { return b.settings.type != FilterType::bell; }))
    {
        const auto bellsOnly = addBells ({});
        if (ctx.cost (ctx.model (bellsOnly)) < ctx.cost (ctx.model (bands)))
            bands = bellsOnly;
    }

    std::vector<BandSettings> result;
    for (const auto& b : bands)
        result.push_back (b.settings);
    return result;
}
