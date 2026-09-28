#pragma once

#include <array>

//==============================================================================
/** Stereo peak and RMS levels for the output meter (UI side, fed from the post tap).

    RMS: exponential average with a 300 ms time constant. Peak: the highest
    sample, held for 1 s, then falling at 20 dB/s. Clip: any sample above
    0 dBFS lights it until resetClip().
*/
class LevelMeter
{
public:
    static constexpr double rmsSeconds = 0.3;
    static constexpr double holdSeconds = 1.0;
    static constexpr double peakFallDbPerSecond = 20.0;
    static constexpr double floorDb = -100.0;

    void addSamples (const float* left, const float* right, int numSamples, double sampleRate);

    /** Advances peak hold and fall by elapsedSeconds. */
    void update (double elapsedSeconds);

    double peakDb (int channel) const noexcept;
    double rmsDb (int channel) const noexcept;
    bool isClipped (int channel) const noexcept { return clipped[static_cast<size_t> (channel)]; }
    void resetClip() noexcept { clipped = { false, false }; }

private:
    std::array<double, 2> meanSquare {}, peak { floorDb, floorDb }, held { floorDb, floorDb };
    std::array<double, 2> holdRemaining {};
    std::array<bool, 2> clipped {};
};
