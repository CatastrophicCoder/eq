#pragma once

#include "Parameters.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

//==============================================================================
/** Small helpers for tests that drive the processor through its parameters. */
namespace TestParameters
{
    inline juce::RangedAudioParameter& param (ParametricEQAudioProcessor& p, const juce::String& id)
    {
        auto* parameter = p.getValueTreeState().getParameter (id);
        REQUIRE (parameter != nullptr);
        return *parameter;
    }

    inline float value (ParametricEQAudioProcessor& p, const juce::String& id)
    {
        return p.getValueTreeState().getRawParameterValue (id)->load();
    }

    inline void set (ParametricEQAudioProcessor& p, const juce::String& id, float newValue)
    {
        auto& parameter = param (p, id);
        parameter.setValueNotifyingHost (parameter.convertTo0to1 (newValue));
    }

    /** Sets every parameter of one band and marks it in use (enabled or disabled). */
    inline void setBand (ParametricEQAudioProcessor& p, int band, FilterType type, float freq,
                         float gain, float q, int slope, bool enabled)
    {
        p.setBandInUse (band, true);
        set (p, Parameters::id (band, "type"), static_cast<float> (type));
        set (p, Parameters::id (band, "freq"), freq);
        set (p, Parameters::id (band, "gain"), gain);
        set (p, Parameters::id (band, "q"), q);
        set (p, Parameters::id (band, "slope"), static_cast<float> (slope));
        set (p, Parameters::id (band, "enabled"), enabled ? 1.0f : 0.0f);
    }
}
