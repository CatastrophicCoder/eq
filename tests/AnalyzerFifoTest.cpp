#include "dsp/AnalyzerFifo.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>

#include <juce_core/juce_core.h>

TEST_CASE ("AnalyzerFifo passes samples through in order", "[fifo]")
{
    AnalyzerFifo fifo;
    fifo.prepare (2, 1000);
    CHECK (fifo.getCapacity() == 1000);
    CHECK (fifo.getNumChannels() == 2);

    std::vector<float> left (300), right (300);
    for (int i = 0; i < 300; ++i) { left[static_cast<size_t> (i)] = static_cast<float> (i); right[static_cast<size_t> (i)] = -static_cast<float> (i); }
    const float* channels[] { left.data(), right.data() };

    CHECK (fifo.push (channels, 2, 300) == 300);
    CHECK (fifo.getNumReady() == 300);

    juce::AudioBuffer<float> out (2, 200);
    CHECK (fifo.pull (out) == 200);
    for (int i = 0; i < 200; ++i)
    {
        REQUIRE (juce::exactlyEqual (out.getSample (0, i), static_cast<float> (i)));
        REQUIRE (juce::exactlyEqual (out.getSample (1, i), -static_cast<float> (i)));
    }

    CHECK (fifo.pull (out) == 100);
    CHECK (juce::exactlyEqual (out.getSample (0, 0), 200.0f));
    CHECK (fifo.getNumReady() == 0);
}

TEST_CASE ("AnalyzerFifo drops what does not fit instead of waiting", "[fifo]")
{
    AnalyzerFifo fifo;
    fifo.prepare (1, 256);

    std::vector<float> data (400, 1.0f);
    const float* channels[] { data.data() };

    CHECK (fifo.push (channels, 1, 400) == 256);   // full: the rest is dropped
    CHECK (fifo.push (channels, 1, 10) == 0);
    CHECK (fifo.getNumReady() == 256);

    fifo.reset();
    CHECK (fifo.getNumReady() == 0);
}

TEST_CASE ("AnalyzerFifo keeps order across a producer and a consumer thread", "[fifo]")
{
    // The plugin's use: the audio thread pushes, the UI thread pulls. Here the producer
    // retries until everything is written, so every sample must arrive, in order.
    constexpr int total = 500000;
    AnalyzerFifo fifo;
    fifo.prepare (1, 4096);

    std::atomic<bool> producerDone { false }, giveUp { false };

    // A broken FIFO must fail this test, not hang it: both sides give up after a deadline.
    const auto deadline = juce::Time::getMillisecondCounter() + 20000;

    std::thread producer ([&]
    {
        juce::Random random (5);
        std::vector<float> chunk (700);
        int next = 0;

        while (next < total)
        {
            const auto n = std::min (1 + random.nextInt (700), total - next);
            for (int i = 0; i < n; ++i)
                chunk[static_cast<size_t> (i)] = static_cast<float> (next + i);

            int written = 0;
            while (written < n && ! giveUp)
            {
                const float* channels[] { chunk.data() + written };
                const auto w = fifo.push (channels, 1, n - written);
                written += w;
                if (w == 0)
                {
                    std::this_thread::yield();
                    if (juce::Time::getMillisecondCounter() > deadline)
                        giveUp = true;
                }
            }

            if (giveUp)
                break;

            next += n;
        }

        producerDone = ! giveUp;
    });

    juce::AudioBuffer<float> out (1, 1000);
    int expected = 0;
    bool ordered = true;

    while (expected < total && ordered && ! giveUp)
    {
        const auto n = fifo.pull (out);
        for (int i = 0; i < n; ++i)
        {
            ordered = ordered && juce::exactlyEqual (out.getSample (0, i), static_cast<float> (expected));
            ++expected;
        }

        if (n == 0)
        {
            std::this_thread::yield();
            if (juce::Time::getMillisecondCounter() > deadline)
                giveUp = true;
        }
    }

    giveUp = true;   // release the producer if the consumer stopped early
    producer.join();
    CHECK (ordered);
    CHECK (expected == total);
    CHECK (producerDone);
}
