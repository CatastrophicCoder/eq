#include "LinearPhaseUpdater.h"

LinearPhaseUpdater::LinearPhaseUpdater (LinearPhaseEngine& e, std::function<Request()> read)
    : juce::Thread ("Linear phase"), engine (e), readRequest (std::move (read))
{
}

LinearPhaseUpdater::~LinearPhaseUpdater()
{
    stopThread (4000);
}

void LinearPhaseUpdater::start()
{
    startThread (juce::Thread::Priority::low);
}

bool LinearPhaseUpdater::sameFilter (const Request& a, const Request& b) noexcept
{
    if (a.numTaps != b.numTaps || ! juce::exactlyEqual (a.sampleRate, b.sampleRate))
        return false;

    for (size_t i = 0; i < a.bands.size(); ++i)
    {
        const auto& x = a.bands[i];
        const auto& y = b.bands[i];
        const auto inX = x.isActive() && ! x.isDynamic();
        const auto inY = y.isActive() && ! y.isDynamic();

        if (inX != inY)
            return false;

        if (inX && ! (x.type == y.type && x.slopeIndex == y.slopeIndex && x.channel == y.channel
                      && juce::exactlyEqual (x.frequencyHz, y.frequencyHz) && juce::exactlyEqual (x.gainDb, y.gainDb)
                      && juce::exactlyEqual (x.q, y.q)))
            return false;
    }

    return true;
}

void LinearPhaseUpdater::run()
{
    Request submitted;
    bool haveSubmitted = false, pending = false;
    LinearPhaseDesigner::Result result;
    Request designed;

    while (! threadShouldExit())
    {
        const auto request = readRequest();

        if (request.linear)
        {
            if (! pending && ! (haveSubmitted && sameFilter (request, submitted)))
            {
                designer.design (request.bands, request.sampleRate, request.numTaps, result);
                designed = request;
                pending = true;
            }

            if (pending && engine.submit (result))
            {
                submitted = designed;
                haveSubmitted = true;
                pending = false;
                continue;   // check again at once: settings may have moved during the design
            }
        }

        wait (pollIntervalMs);
    }
}
