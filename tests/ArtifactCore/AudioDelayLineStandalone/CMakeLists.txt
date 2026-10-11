cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioDelayLineStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioDelayLineModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Audio/DSP/DelayLine.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreAudioDelayLineStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioDelayLineStandaloneTest.cpp"
    LINK_LIBS AudioDelayLineModuleInterfaces
    LABELS "audio;dsp;delay-line;cpu;contract;standalone"
)

enable_testing()
