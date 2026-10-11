cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioLFOSolo LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioLFOModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Audio/DSP/LFO.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreAudioLFOSoloTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioLFOSoloTest.cpp"
    LINK_LIBS AudioLFOModuleInterfaces
    LABELS "audio;dsp;lfo;cpu;contract;standalone"
)

enable_testing()
