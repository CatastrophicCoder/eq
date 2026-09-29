add_executable(SpectralFaultTests
    ProcessorSmokeTest.cpp
    MatchedPeakingDesignTest.cpp
    StateTest.cpp
    SecondOrderShapesTest.cpp
    ShelfDesignTest.cpp
    CutDesignTest.cpp
    CascadeProcessorTest.cpp
    BandDesignTest.cpp
    EqBandTest.cpp
    RealtimeAllocationTest.cpp
    ParametersTest.cpp
    ProcessorMultiBandTest.cpp
    AutoGainTest.cpp
    OutputStageTest.cpp
    EditorTest.cpp
    FrequencyAxisTest.cpp
    ResponseCurvesTest.cpp
    SpectrumColourTest.cpp
    InteractionTest.cpp
    AnalyzerFifoTest.cpp
    SpectrumAnalyzerTest.cpp
    LevelMeterTest.cpp
    AnalyzerProcessorTest.cpp
    ChannelModeTest.cpp
    PresetTest.cpp
    DynamicsTest.cpp
    DynamicsProcessingTest.cpp
    LinearPhaseDesignTest.cpp
    LinearPhaseProcessingTest.cpp
    AbComparisonTest.cpp
    UndoHistoryTest.cpp
    PeakPickTest.cpp
    CurveFitterTest.cpp
    MatchTest.cpp
    ScreenshotTest.cpp
    SpectralDynamicsTest.cpp)

# Link against the plugin's shared-code library. Its JUCE modules are linked
# PRIVATE there, so take its include paths and defines explicitly instead of
# relinking the modules (which would compile JUCE a second time).
target_include_directories(SpectralFaultTests
    PRIVATE
        $<TARGET_PROPERTY:SpectralFault,INCLUDE_DIRECTORIES>)

target_compile_definitions(SpectralFaultTests
    PRIVATE
        $<TARGET_PROPERTY:SpectralFault,COMPILE_DEFINITIONS>)

target_link_libraries(SpectralFaultTests
    PRIVATE
        SpectralFault
        Catch2::Catch2WithMain)

include(${PROJECT_SOURCE_DIR}/external/Catch2/extras/Catch.cmake)
catch_discover_tests(SpectralFaultTests)
