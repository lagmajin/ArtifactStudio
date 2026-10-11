cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAllPassFilterStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AllPassFilterModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/DSP/DelayLine.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/DSP/AllPassFilter.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreAllPassFilterStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AllPassFilterStandaloneTest.cpp"
    LINK_LIBS AllPassFilterModuleInterfaces
    LABELS "audio;dsp;allpass;filter;cpu;contract;standalone"
)

enable_testing()
