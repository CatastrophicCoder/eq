#include "SpectralDynamicsEngine.h"

void SpectralDynamicsEngine::prepare (double rate) { sampleRate = rate; }
void SpectralDynamicsEngine::reset() noexcept {}
void SpectralDynamicsEngine::setBands (const std::array<BandSettings, numBands>& b) noexcept { bands = b; }
void SpectralDynamicsEngine::process (juce::AudioBuffer<float>&, const juce::AudioBuffer<float>*) noexcept {}
float SpectralDynamicsEngine::getSliceGainDb (int, int) const noexcept { return 0.0f; }
double SpectralDynamicsEngine::regionWeight (const BandSettings&, double) noexcept { return 0.0; }
bool SpectralDynamicsEngine::anySpectral (const std::array<BandSettings, numBands>& b) noexcept
{
    for (const auto& s : b)
        if (s.isSpectral())
            return true;
    return false;
}
void SpectralDynamicsEngine::processFrame() noexcept {}
