add_executable(ParametricEQTests
    ProcessorSmokeTest.cpp
    MatchedPeakingDesignTest.cpp
    PeakingBandTest.cpp
    StateTest.cpp
    SecondOrderShapesTest.cpp
    ShelfDesignTest.cpp
    CutDesignTest.cpp
    CascadeProcessorTest.cpp)

# Link against the plugin's shared-code library. Its JUCE modules are linked
# PRIVATE there, so take its include paths and defines explicitly instead of
# relinking the modules (which would compile JUCE a second time).
target_include_directories(ParametricEQTests
    PRIVATE
        $<TARGET_PROPERTY:ParametricEQ,INCLUDE_DIRECTORIES>)

target_compile_definitions(ParametricEQTests
    PRIVATE
        $<TARGET_PROPERTY:ParametricEQ,COMPILE_DEFINITIONS>)

target_link_libraries(ParametricEQTests
    PRIVATE
        ParametricEQ
        Catch2::Catch2WithMain)

include(${PROJECT_SOURCE_DIR}/external/Catch2/extras/Catch.cmake)
catch_discover_tests(ParametricEQTests)
