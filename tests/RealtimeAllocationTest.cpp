// Counts heap allocations while a flag is set, by replacing the global allocation
// functions for the whole test binary. Used to check the real-time rule
// "no memory allocation in processBlock and anything it calls".

#include "dsp/EqBand.h"
#include "dsp/CutSlope.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdlib>
#include <new>

namespace
{
    std::atomic<bool> counting { false };
    std::atomic<int> allocations { 0 };

    void* allocate (std::size_t size, std::size_t alignment = 0)
    {
        if (counting.load (std::memory_order_relaxed))
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
    ScopedAllocationCounter counter;
    auto* p = new int (42);
    delete p;
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
