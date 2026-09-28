cmake_minimum_required(VERSION 3.22)

project(ParametricEQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PARAMETRICEQ_BUILD_TESTS "Build the Catch2 unit tests" ON)

add_subdirectory(external/JUCE)

juce_add_plugin(ParametricEQ
    COMPANY_NAME "CatastrophicCoder"
    BUNDLE_ID com.catastrophiccoder.parametriceq
    PLUGIN_MANUFACTURER_CODE Ctcd
    PLUGIN_CODE Peq1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "ParametricEQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(ParametricEQ
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/Parameters.cpp
        src/AutoGainUpdater.cpp
        src/ui/FrequencyAxis.cpp
        src/ui/ResponseCurves.cpp
        src/ui/ResponseDisplay.cpp
        src/ui/SpectrumColour.cpp
        src/ui/NodeLayout.cpp
        src/ui/SelectionModel.cpp
        src/ui/NodeDragController.cpp
        src/ui/BandParameterWriter.cpp
        src/ui/SpectrumAnalyzer.cpp
        src/ui/LevelMeter.cpp
        src/ui/BandPanel.cpp
        src/ui/TopBar.cpp
        src/ui/BottomBar.cpp
        src/ui/CompactLookAndFeel.cpp
        src/dsp/BiquadCoefficients.cpp
        src/dsp/MatchedPeakingDesign.cpp
        src/dsp/SectionCascade.cpp
        src/dsp/MatchedLowpassDesign.cpp
        src/dsp/MatchedHighpassDesign.cpp
        src/dsp/MatchedBandpassDesign.cpp
        src/dsp/MatchedNotchDesign.cpp
        src/dsp/MatchedAllpassDesign.cpp
        src/dsp/MatchedShelfDesign.cpp
        src/dsp/MatchedOnePoleShelfDesign.cpp
        src/dsp/TiltShelfDesign.cpp
        src/dsp/FirstOrderCutDesign.cpp
        src/dsp/ButterworthCascade.cpp
        src/dsp/FlatTiltDesign.cpp
        src/dsp/CascadeProcessor.cpp
        src/dsp/BandDesign.cpp
        src/dsp/EqBand.cpp
        src/dsp/KWeighting.cpp
        src/dsp/AutoGain.cpp
        src/dsp/AnalyzerFifo.cpp)

target_include_directories(ParametricEQ
    PUBLIC
        src)

target_compile_definitions(ParametricEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ParametricEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (PARAMETRICEQ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(external/Catch2)
    add_subdirectory(tests)
endif()
