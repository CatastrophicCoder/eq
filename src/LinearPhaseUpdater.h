#pragma once

#include "dsp/LinearPhaseDesigner.h"
#include "dsp/LinearPhaseEngine.h"

#include <juce_core/juce_core.h>

#include <functional>

//==============================================================================
/** Background thread that keeps the linear-phase filter current (M8).

    Every pollIntervalMs, while Linear phase is requested, it compares the parts of
    the band settings that shape the filter (active, non-dynamic bands), the length
    and the sample rate with the last design; on a change it designs a new filter
    and submits it to the engine (retrying while the engine's slot is still full).
*/
class LinearPhaseUpdater final : private juce::Thread
{
public:
    static constexpr int pollIntervalMs = 20;

    struct Request
    {
        bool linear = false;
        int numTaps = LinearPhaseDesigner::tapCounts[0];
        double sampleRate = 48000.0;
        std::array<BandSettings, 16> bands {};
    };

    LinearPhaseUpdater (LinearPhaseEngine& engine, std::function<Request()> readRequest);
    ~LinearPhaseUpdater() override;

    /** Starts the thread; call once the request source is fully constructed. */
    void start();

    /** True if two requests would give the same filter. */
    static bool sameFilter (const Request& a, const Request& b) noexcept;

private:
    void run() override;

    LinearPhaseEngine& engine;
    std::function<Request()> readRequest;
    LinearPhaseDesigner designer;
};
