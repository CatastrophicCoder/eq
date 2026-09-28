// Counts heap allocations while a flag is set, by replacing the global allocation
// functions for the whole test binary. Used to check the real-time rule
// "no memory allocation in processBlock and anything it calls".

#include "TestParameters.h"
#include "dsp/EqBand.h"
#include "dsp/CutSlope.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdlib>
#include <new>
#include <vector>

namespace
{
    // Counts only on the thread that opened the counter (the "audio" thread in these tests):
    // background threads such as the linear-phase designer allocate by design.
    thread_local bool counting = false;
    std::atomic<int> allocations { 0 };

    void* allocate (std::size_t size, std::size_t alignment = 0)
    {
        if (counting)
            allocations.fetch_add (1, std::memory_order_relaxed);

        void* p = nullptr;

        if (alignment > alignof (std::max_align_t))
        {
            if (posix_memalign (&p, alignment, size == 0 ? 1 : size) != 0)
                p = nullptr;
        }
        else
        {
            p = std::malloc (size == 0 ? 1 : size);
        }

        if (p == nullptr)
            throw std::bad_alloc();

        return p;
    }

    struct ScopedAllocationCounter
    {
        ScopedAllocationCounter()  { allocations = 0; counting = true; }
        ~ScopedAllocationCounter() { counting = false; }
        int count() const          { return allocations.load(); }
    };
}

void* operator new (std::size_t size)                                   { return allocate (size); }
void* operator new[] (std::size_t size)                                 { return allocate (size); }
void* operator new (std::size_t size, std::align_val_t a)               { return allocate (size, static_cast<std::size_t> (a)); }
void* operator new[] (std::size_t size, std::align_val_t a)             { return allocate (size, static_cast<std::size_t> (a)); }
void operator delete (void* p) noexcept                                 { std::free (p); }
void operator delete[] (void* p) noexcept                               { std::free (p); }
void operator delete (void* p, std::size_t) noexcept                    { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept                  { std::free (p); }
void operator delete (void* p, std::align_val_t) noexcept               { std::free (p); }
void operator delete[] (void* p, std::align_val_t) noexcept             { std::free (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept  { std::free (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { std::free (p); }

TEST_CASE ("Allocation counter sees allocations", "[realtime]")
{
    // A plain new-expression may be optimised away in Release (C++14 allows eliding new/delete
    // pairs); a direct call of the allocation function may not.
    ScopedAllocationCounter counter;
    void* p = ::operator new (64);
    ::operator delete (p);
    CHECK (counter.count() >= 1);
}

TEST_CASE ("EqBand::process does not allocate, including ramps and crossfades", "[realtime]")
{
    BandSettings s;
    s.type = FilterType::bell;
    s.gainDb = 6.0;

    EqBand band;
    band.setTargets (s);
    band.prepare (48000.0, 2);

    juce::AudioBuffer<float> buffer (2, 256);
    buffer.clear();

    ScopedAllocationCounter counter;

    for (int block = 0; block < 200; ++block)
    {
        if (block % 10 == 0)
        {
            s.type = static_cast<FilterType> ((block / 10) % FilterTypes::count);
            s.slopeIndex = (block / 10) % CutSlope::count;
            s.frequencyHz = 100.0 + 50.0 * block;
            s.enabled = (block / 10) % 4 != 3;
            band.setTargets (s);
        }

        band.process (buffer);
    }

    CHECK (counter.count() == 0);
}

TEST_CASE ("Processor processBlock does not allocate with 16 active bands", "[realtime]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    ParametricEQAudioProcessor processor;

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        TestParameters::setBand (processor, band, static_cast<FilterType> (band % FilterTypes::count),
                                 50.0f * static_cast<float> (band), 3.0f, 1.0f, band % CutSlope::count, true);
        TestParameters::set (processor, Parameters::id (band, "channel"), static_cast<float> (band % ChannelModes::count));
    }

    TestParameters::set (processor, Parameters::autoGain, 1.0f);

    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);
    processor.setAnalyzerActive (true);   // taps on: pushing to the FIFOs must not allocate either

    juce::AudioBuffer<float> buffer (2, 512);
    buffer.clear();
    juce::MidiBuffer midi;

    auto* outputGain = processor.getValueTreeState().getParameter (Parameters::outputGain);
    auto* invert = processor.getValueTreeState().getParameter (Parameters::outputInvert);

    // Look parameters up before counting: building ID strings allocates, and a host holds
    // parameter pointers rather than looking them up per block.
    struct Automated { juce::RangedAudioParameter* gain; juce::RangedAudioParameter* type; juce::RangedAudioParameter* enabled; };
    std::vector<Automated> automated;
    for (int band = 1; band <= Parameters::numBands; ++band)
        automated.push_back ({ processor.getValueTreeState().getParameter (Parameters::id (band, "gain")),
                               processor.getValueTreeState().getParameter (Parameters::id (band, "type")),
                               processor.getValueTreeState().getParameter (Parameters::id (band, "enabled")) });

    ScopedAllocationCounter counter;

    for (int block = 0; block < 100; ++block)
    {
        // Raw normalised values, as a host sends automation: gain, type and enable changes.
        if (block % 10 == 0)
        {
            const auto& a = automated[static_cast<size_t> ((block / 10) % Parameters::numBands)];
            a.gain->setValue (static_cast<float> (block % 20) / 20.0f);
            a.type->setValue (static_cast<float> (block % 30) / 30.0f);
            a.enabled->setValue (block % 40 < 20 ? 1.0f : 0.0f);
            outputGain->setValue (static_cast<float> (block % 50) / 50.0f);
            invert->setValue (block % 20 < 10 ? 1.0f : 0.0f);
        }

        processor.processBlock (buffer, midi);
    }

    CHECK (counter.count() == 0);
}


TEST_CASE ("Processor processBlock does not allocate with 16 dynamic bands and a side-chain", "[realtime][dynamics]")
{
    juce::ScopedJuceInitialiser_GUI juce;

    ParametricEQAudioProcessor processor;
    const FilterType dynamicTypes[] { FilterType::bell, FilterType::lowShelf, FilterType::highShelf };

    for (int band = 1; band <= Parameters::numBands; ++band)
    {
        TestParameters::setBand (processor, band, dynamicTypes[band % 3], 60.0f * static_cast<float> (band), 2.0f, 1.0f, 1, true);
        TestParameters::set (processor, Parameters::id (band, "channel"), static_cast<float> (band % ChannelModes::count));
        TestParameters::set (processor, Parameters::id (band, "dyn"), 1.0f);
        TestParameters::set (processor, Parameters::id (band, "thresh"), -50.0f);
        TestParameters::set (processor, Parameters::id (band, "dynmode"), static_cast<float> (band % 2));
        TestParameters::set (processor, Parameters::id (band, "detector"), static_cast<float> ((band / 2) % 2));
        TestParameters::set (processor, Parameters::id (band, "sidechain"), band % 3 == 0 ? 1.0f : 0.0f);
    }

    auto layout = processor.getBusesLayout();
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    REQUIRE (processor.setBusesLayout (layout));
    processor.prepareToPlay (48000.0, 512);
    processor.setAnalyzerActive (true);

    // Main (channels 0-1) and side-chain (2-3): noise, loud enough to move every detector.
    juce::AudioBuffer<float> buffer (4, 512);
    juce::Random random (3);
    juce::MidiBuffer midi;

    struct Automated { juce::RangedAudioParameter* freq; juce::RangedAudioParameter* range; juce::RangedAudioParameter* dyn; };
    std::vector<Automated> automated;
    for (int band = 1; band <= Parameters::numBands; ++band)
        automated.push_back ({ processor.getValueTreeState().getParameter (Parameters::id (band, "freq")),
                               processor.getValueTreeState().getParameter (Parameters::id (band, "range")),
                               processor.getValueTreeState().getParameter (Parameters::id (band, "dyn")) });

    ScopedAllocationCounter counter;

    for (int block = 0; block < 100; ++block)
    {
        for (int ch = 0; ch < 4; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.5f * (random.nextFloat() - 0.5f));

        if (block % 10 == 0)
        {
            const auto& a = automated[static_cast<size_t> ((block / 10) % Parameters::numBands)];
            a.freq->setValue (static_cast<float> (block % 30) / 30.0f);
            a.range->setValue (static_cast<float> (block % 20) / 20.0f);
            a.dyn->setValue (block % 40 < 20 ? 1.0f : 0.0f);
        }

        processor.processBlock (buffer, midi);
    }

    CHECK (counter.count() == 0);
}

TEST_CASE ("AnalyzerFifo::push does not allocate", "[realtime]")
{
    AnalyzerFifo fifo;
    fifo.prepare (2, 4096);
    std::vector<float> data (512, 0.25f);
    const float* channels[] { data.data(), data.data() };

    ScopedAllocationCounter counter;
    for (int i = 0; i < 20; ++i)
        fifo.push (channels, 2, 512);   // fills, then drops

    CHECK (counter.count() == 0);
}


TEST_CASE ("Processor processBlock does not allocate in Linear phase mode, including filter swaps", "[realtime][linearphase]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    ParametricEQAudioProcessor processor;
    TestParameters::setBand (processor, 2, FilterType::bell, 800.0f, 4.0f, 1.0f, 3, true);
    TestParameters::setBand (processor, 5, FilterType::highShelf, 6000.0f, -3.0f, 0.71f, 3, true);
    TestParameters::set (processor, Parameters::id (5, "channel"), static_cast<float> (ChannelMode::side));   // cross terms
    TestParameters::setBand (processor, 7, FilterType::bell, 2000.0f, 0.0f, 2.0f, 3, true);
    TestParameters::set (processor, "band7_dyn", 1.0f);                                                        // IIR after the FIR
    processor.setLinearPhase (true);
    processor.setLinearPhaseLength (0);
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::Random random (4);
    juce::MidiBuffer midi;
    auto* gain = processor.getValueTreeState().getParameter ("band2_gain");

    // Let the first filter settle, then count while parameters change and new filters arrive.
    for (int block = 0; block < 200 && ! processor.isPhaseModeSettled(); ++block)
    {
        processor.processBlock (buffer, midi);
        juce::Thread::sleep (2);
    }
    REQUIRE (processor.isPhaseModeSettled());

    ScopedAllocationCounter counter;

    for (int block = 0; block < 300; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.25f * (random.nextFloat() - 0.5f));

        if (block % 40 == 0)
            gain->setValue (static_cast<float> (block % 120) / 120.0f);   // triggers a redesign and swap

        processor.processBlock (buffer, midi);
        juce::Thread::sleep (1);   // give the designer and loader threads time
    }

    CHECK (counter.count() == 0);
    CHECK (processor.getLinearPhaseSwapCount() > 1);   // filters really were swapped while counting
}