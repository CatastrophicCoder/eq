#pragma once

//==============================================================================
/** Level detector for dynamic bands (M7): Peak (rectified signal) or RMS (10 ms
    mean square), followed by a classic attack/release envelope on the linear
    level (one-pole each, time constant = the attack or release time), then
    converted to dB (decision 2026-09-29; replaces smoothing in the dB domain).
    After the signal stops, the level falls 8.69 dB per release time.
*/
class LevelDetector
{
public:
    enum class Mode { peak, rms };

    static constexpr double rmsSeconds = 0.01;
    static constexpr double floorDb = -120.0;

    void prepare (double sampleRate);
    void setTimes (double attackMs, double releaseMs) noexcept;
    void setMode (Mode newMode) noexcept { mode = newMode; }
    void reset() noexcept;

    /** One sample in; the envelope in dB out. */
    double process (double input) noexcept;

    double getLevelDb() const noexcept { return envelopeDb; }

private:
    double sampleRate = 48000.0;
    Mode mode = Mode::peak;
    double attackCoeff = 0.0, releaseCoeff = 0.0, rmsCoeff = 0.0;
    double meanSquare = 0.0, envelope = 0.0, envelopeDb = floorDb;
};
