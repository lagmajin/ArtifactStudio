cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioModulatorsStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioModulatorModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Audio/Modulation/Modulator.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreAudioModulatorsStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioModulatorsStandaloneTest.cpp"
    LINK_LIBS AudioModulatorModuleInterfaces
    LABELS "core;audio;modulation;cpu;contract;standalone"
)

enable_testing()
