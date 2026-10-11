cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioModulationRouterStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioModulationRouterModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/Modulation/Modulator.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Modulation/Router.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreAudioModulationRouterStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioModulationRouterStandaloneTest.cpp"
    LINK_LIBS AudioModulationRouterModuleInterfaces
    LABELS "core;audio;modulation;router;cpu;contract;standalone"
)

enable_testing()
